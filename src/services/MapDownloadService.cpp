#include "MapDownloadService.h"

#include "utils/ZstdDecompressor.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QStorageInfo>
#include <QDebug>
#include <QtConcurrent>

#include <cstdio>

#ifdef Q_OS_LINUX
#include <QProcess>
#endif

const QHash<QString, QString> MapDownloadService::s_stateToSlug = {
    {QStringLiteral("Baden-Württemberg"), QStringLiteral("baden-wuerttemberg")},
    {QStringLiteral("Bayern"), QStringLiteral("bayern")},
    {QStringLiteral("Berlin"), QStringLiteral("berlin_brandenburg")},
    {QStringLiteral("Brandenburg"), QStringLiteral("berlin_brandenburg")},
    {QStringLiteral("Bremen"), QStringLiteral("bremen")},
    {QStringLiteral("Hamburg"), QStringLiteral("hamburg")},
    {QStringLiteral("Hessen"), QStringLiteral("hessen")},
    {QStringLiteral("Mecklenburg-Vorpommern"), QStringLiteral("mecklenburg-vorpommern")},
    {QStringLiteral("Niedersachsen"), QStringLiteral("niedersachsen")},
    {QStringLiteral("Nordrhein-Westfalen"), QStringLiteral("nordrhein-westfalen")},
    {QStringLiteral("Rheinland-Pfalz"), QStringLiteral("rheinland-pfalz")},
    {QStringLiteral("Saarland"), QStringLiteral("saarland")},
    {QStringLiteral("Sachsen"), QStringLiteral("sachsen")},
    {QStringLiteral("Sachsen-Anhalt"), QStringLiteral("sachsen-anhalt")},
    {QStringLiteral("Schleswig-Holstein"), QStringLiteral("schleswig-holstein")},
    {QStringLiteral("Thüringen"), QStringLiteral("thueringen")},
};

MapDownloadService::MapDownloadService(bool simulatorMode, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_simulatorMode(simulatorMode)
{
    m_metadata = MapMetadata::load();
    if (!m_metadata.region.isEmpty()) {
        m_resolvedSlug = m_metadata.region;
        // Restore friendly name so the QML's `dlRegion === ""` guard does not
        // trigger a redundant Nominatim re-detect on every boot.
        m_regionName = displayNameForSlug(m_resolvedSlug);
    }
    m_updateAvailable = m_metadata.updateAvailable;

    adoptInstalledMaps();
    computeMissingDigests();

    // Check for partial downloads
    emit partialStateChanged();
}

bool MapDownloadService::hasPartialDisplayDownload() const
{
    return QFile::exists(displayPartPath());
}

bool MapDownloadService::hasPartialRoutingDownload() const
{
    return QFile::exists(routingPartPath()) || QFile::exists(routingCompressedPartPath());
}

void MapDownloadService::setStatus(ScootEnums::MapDownloadStatus s)
{
    if (m_status != s) {
        m_status = s;
        emit statusChanged();
    }
}

void MapDownloadService::setError(const QString &msg)
{
    m_errorMessage = msg;
    emit errorMessageChanged();
    setStatus(ScootEnums::MapDownloadStatus::Error);
}

void MapDownloadService::resolveRegion(double lat, double lng)
{
    if (m_status != ScootEnums::MapDownloadStatus::Idle &&
        m_status != ScootEnums::MapDownloadStatus::Error)
        return;

    m_cancelled = false;
    setStatus(ScootEnums::MapDownloadStatus::Locating);
    doResolveSlug(lat, lng);
}

void MapDownloadService::startDownload(double lat, double lng, bool needsDisplay, bool needsRouting)
{
    if (m_status != ScootEnums::MapDownloadStatus::Idle &&
        m_status != ScootEnums::MapDownloadStatus::Error)
        return;

    m_cancelled = false;
    m_needsDisplay = needsDisplay;
    m_needsRouting = needsRouting;
    m_displayDone = !needsDisplay;
    m_routingDone = !needsRouting;

    if (m_resolvedSlug.isEmpty()) {
        setStatus(ScootEnums::MapDownloadStatus::Locating);
        doResolveSlug(lat, lng);
    } else {
        setStatus(ScootEnums::MapDownloadStatus::CheckingUpdates);
        doFetchReleases(needsDisplay, needsRouting);
    }
}

void MapDownloadService::cancel()
{
    m_cancelled = true;
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply = nullptr;
    }
    if (m_currentFile) {
        m_currentFile->close();
        m_currentFile->deleteLater();
        m_currentFile = nullptr;
    }
    m_progress = 0.0;
    m_downloadedBytes = 0;
    m_totalBytes = 0;
    m_completedBytes = 0;
    emit progressChanged();
    setStatus(ScootEnums::MapDownloadStatus::Idle);
}

void MapDownloadService::checkForUpdatesAt(double lat, double lng)
{
    if (!m_resolvedSlug.isEmpty()) {
        checkForUpdates();
        return;
    }

    if (m_status != ScootEnums::MapDownloadStatus::Idle &&
        m_status != ScootEnums::MapDownloadStatus::Error)
        return;

    // Resolve first, then check. doResolveSlug picks the check back up.
    m_cancelled = false;
    m_pendingUpdateCheck = true;
    setStatus(ScootEnums::MapDownloadStatus::Locating);
    doResolveSlug(lat, lng);
}

void MapDownloadService::checkForUpdatesNow()
{
    double lat = 0.0, lng = 0.0;
    if (m_resolvedSlug.isEmpty() && m_positionProvider && m_positionProvider(lat, lng)) {
        checkForUpdatesAt(lat, lng);
        return;
    }
    checkForUpdates();
}

void MapDownloadService::checkForUpdates()
{
    if (m_status != ScootEnums::MapDownloadStatus::Idle &&
        m_status != ScootEnums::MapDownloadStatus::Error)
        return;

    m_cancelled = false;
    setStatus(ScootEnums::MapDownloadStatus::CheckingUpdates);

    fetchTilesManifest([this](const QJsonObject &manifest) {
        if (manifest.isEmpty()) {
            setStatus(ScootEnums::MapDownloadStatus::Idle);
            emit updateCheckCompleted(false);
            return;
        }

        // No region on record: this vehicle's maps were installed by the
        // flasher. Identify them by digest before comparing anything.
        if (m_resolvedSlug.isEmpty() && !adoptRegionFromManifest(manifest)) {
            qDebug() << "Map update check: region unknown and digests match no "
                        "published region, skipping";
            setStatus(ScootEnums::MapDownloadStatus::Idle);
            emit updateCheckCompleted(false);
            return;
        }

        auto region = manifest[m_resolvedSlug].toObject();
        if (region.isEmpty()) {
            setStatus(ScootEnums::MapDownloadStatus::Idle);
            emit updateCheckCompleted(false);
            return;
        }

        bool hasUpdate = false;
        if (m_metadata.displayTiles && !m_metadata.displayTiles->digest.isEmpty()) {
            QString remoteDigest = region[QStringLiteral("map")].toObject()
                                       [QStringLiteral("sha256")].toString();
            if (!remoteDigest.isEmpty() && remoteDigest != m_metadata.displayTiles->digest) {
                hasUpdate = true;
                qDebug() << "Display map update available:"
                         << m_metadata.displayTiles->digest << "->" << remoteDigest;
            }
        }
        if (m_metadata.valhallaTiles && !m_metadata.valhallaTiles->digest.isEmpty()) {
            QString remoteDigest = region[QStringLiteral("valhalla")].toObject()
                                       [QStringLiteral("sha256")].toString();
            if (!remoteDigest.isEmpty() && remoteDigest != m_metadata.valhallaTiles->digest) {
                hasUpdate = true;
                qDebug() << "Routing map update available:"
                         << m_metadata.valhallaTiles->digest << "->" << remoteDigest;
            }
        }

        m_metadata.lastUpdateCheck = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        MapMetadata::save(m_metadata);

        // Go idle before announcing the update so a direct-connected slot that
        // reacts to updateAvailableChanged (e.g. Application's auto-download
        // wiring) can immediately call startDownload() without tripping the
        // "busy" guard at the top of startDownload().
        setStatus(ScootEnums::MapDownloadStatus::Idle);

        if (hasUpdate != m_updateAvailable) {
            m_updateAvailable = hasUpdate;
            m_metadata.updateAvailable = hasUpdate;
            MapMetadata::save(m_metadata);
            emit updateAvailableChanged();
        }

        emit updateCheckCompleted(hasUpdate);
    });
}

bool MapDownloadService::shouldCheckForUpdates() const
{
    if (!hasMapsInstalled())
        return false;

    if (m_metadata.lastUpdateCheck.isEmpty())
        return true;

    auto lastCheck = QDateTime::fromString(m_metadata.lastUpdateCheck, Qt::ISODate);
    if (!lastCheck.isValid())
        return true;

    return lastCheck.daysTo(QDateTime::currentDateTimeUtc()) >= 7;
}

void MapDownloadService::fetchTilesManifest(std::function<void(const QJsonObject &)> callback)
{
    QUrl url{QStringLiteral("https://downloads.librescoot.org/releases/tiles.json")};
    QNetworkRequest req{url};
    req.setRawHeader("User-Agent", "Librescoot/1.0");
    req.setTransferTimeout(15000);

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback({});
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        callback(doc.object());
    });
}

// --- Pipeline stages ---

void MapDownloadService::doResolveSlug(double lat, double lng)
{
    QString url = QStringLiteral("https://nominatim.openstreetmap.org/reverse?lat=%1&lon=%2&format=json&zoom=5")
                      .arg(lat, 0, 'f', 6).arg(lng, 0, 'f', 6);

    QNetworkRequest req{QUrl{url}};
    req.setRawHeader("User-Agent", "Librescoot/1.0");
    // s_stateToSlug only knows German state names; force Nominatim to return
    // those regardless of the device's system locale.
    req.setRawHeader("Accept-Language", "de");
    req.setTransferTimeout(10000);

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            if (m_pendingUpdateCheck) {
                m_pendingUpdateCheck = false;
                emit updateCheckCompleted(false);
            }
            setError(QStringLiteral("Could not detect region: network error"));
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto address = doc.object()[QStringLiteral("address")].toObject();
        QString state = address[QStringLiteral("state")].toString();
        if (state.isEmpty())
            state = address[QStringLiteral("city")].toString();

        QString slug = slugForState(state);
        if (slug.isEmpty()) {
            if (m_pendingUpdateCheck) {
                m_pendingUpdateCheck = false;
                emit updateCheckCompleted(false);
            }
            setError(QStringLiteral("Unsupported region: ") + state);
            return;
        }

        m_resolvedSlug = slug;
        if (slug == QLatin1String("berlin_brandenburg"))
            m_regionName = QStringLiteral("Berlin/Brandenburg");
        else if (slug == QLatin1String("niedersachsen"))
            m_regionName = QStringLiteral("Niedersachsen (incl. Bremen)");
        else
            m_regionName = state;
        emit regionNameChanged();

        // Persist as soon as it is known. Previously the region was only
        // written after a completed download, so resolving it and then not
        // downloading meant re-resolving on the next boot.
        if (m_metadata.region != m_resolvedSlug) {
            m_metadata.region = m_resolvedSlug;
            MapMetadata::save(m_metadata);
        }

        if (m_pendingUpdateCheck) {
            m_pendingUpdateCheck = false;
            setStatus(ScootEnums::MapDownloadStatus::Idle);
            checkForUpdates();
            return;
        }

        // If we were just resolving (not downloading), fetch sizes then go idle
        if (!m_needsDisplay && !m_needsRouting) {
            fetchEstimates();
            return;
        }

        // Continue to fetch releases
        setStatus(ScootEnums::MapDownloadStatus::CheckingUpdates);
        doFetchReleases(m_needsDisplay, m_needsRouting);
    });
}

void MapDownloadService::fetchEstimates()
{
    fetchTilesManifest([this](const QJsonObject &manifest) {
        if (m_cancelled) return;

        auto region = manifest[m_resolvedSlug].toObject();
        if (!region.isEmpty()) {
            auto map = region[QStringLiteral("map")].toObject();
            if (!map.isEmpty())
                m_estimatedDisplayBytes = static_cast<qint64>(map[QStringLiteral("size")].toDouble());
            auto valhalla = region[QStringLiteral("valhalla")].toObject();
            if (!valhalla.isEmpty())
                m_estimatedRoutingBytes = static_cast<qint64>(valhalla[QStringLiteral("size")].toDouble());
        }
        emit estimatesChanged();
        setStatus(ScootEnums::MapDownloadStatus::Idle);
    });
}

void MapDownloadService::doFetchReleases(bool needsDisplay, bool needsRouting)
{
    fetchTilesManifest([this, needsDisplay, needsRouting](const QJsonObject &manifest) {
        if (m_cancelled) return;

        auto region = manifest[m_resolvedSlug].toObject();
        if (region.isEmpty()) {
            setError(QStringLiteral("Maps not available for ") + m_regionName);
            return;
        }

        qint64 totalNeeded = 0;  // full bytes to download (progress denominator)
        qint64 diskNeeded = 0;   // still-to-write bytes (disk-space check, resume-aware)

        if (needsDisplay) {
            auto map = region[QStringLiteral("map")].toObject();
            if (map.isEmpty()) {
                setError(QStringLiteral("Display maps not available for ") + m_regionName);
                return;
            }
            m_displayAsset.url = map[QStringLiteral("url")].toString();
            m_displayAsset.size = static_cast<qint64>(map[QStringLiteral("size")].toDouble());
            m_displayAsset.digest = map[QStringLiteral("sha256")].toString();
            m_estimatedDisplayBytes = m_displayAsset.size;

            if (m_metadata.displayTiles && !m_metadata.displayTiles->digest.isEmpty()
                && m_metadata.displayTiles->digest == m_displayAsset.digest) {
                // Installed digest already matches the manifest - nothing to fetch.
                m_displayDone = true;
            } else {
                totalNeeded += m_displayAsset.size;
                qint64 partial = QFileInfo(displayPartPath()).size();
                diskNeeded += std::max<qint64>(0, m_displayAsset.size - partial);
            }
        }

        if (needsRouting) {
            auto valhalla = region[QStringLiteral("valhalla")].toObject();
            if (valhalla.isEmpty()) {
                setError(QStringLiteral("Routing maps not available for ") + m_regionName);
                return;
            }
            m_routingAsset.url = valhalla[QStringLiteral("url")].toString();
            m_routingAsset.size = static_cast<qint64>(valhalla[QStringLiteral("size")].toDouble());
            m_routingAsset.digest = valhalla[QStringLiteral("sha256")].toString();
            m_estimatedRoutingBytes = m_routingAsset.size;

            // Optional compressed variant. url/size/sha256 above stay the
            // uncompressed tar, because that is what lands on disk and what the
            // installed-digest comparison in checkForUpdates() is written against.
            const auto compressed = valhalla[QStringLiteral("compressed")].toObject();
            if (!compressed.isEmpty()
                && compressed[QStringLiteral("codec")].toString() == QLatin1String("zstd")) {
                m_routingAsset.compressedUrl = compressed[QStringLiteral("url")].toString();
                m_routingAsset.compressedDigest = compressed[QStringLiteral("sha256")].toString();
                m_routingAsset.compressedSize =
                    static_cast<qint64>(compressed[QStringLiteral("size")].toDouble());
                if (m_routingAsset.compressedUrl.isEmpty() || m_routingAsset.compressedSize <= 0) {
                    // Incomplete entry: fall back to the plain tar rather than guess.
                    m_routingAsset.compressedUrl.clear();
                    m_routingAsset.compressedDigest.clear();
                    m_routingAsset.compressedSize = 0;
                }
            }

            if (m_metadata.valhallaTiles && !m_metadata.valhallaTiles->digest.isEmpty()
                && m_metadata.valhallaTiles->digest == m_routingAsset.digest) {
                m_routingDone = true;
            } else {
                if (m_routingAsset.useCompressed()) {
                    // Transfer is the compressed artifact, but during install
                    // both it and the decompressed tar exist at once.
                    totalNeeded += m_routingAsset.compressedSize;
                    const qint64 partial = QFileInfo(routingCompressedPartPath()).size();
                    diskNeeded += std::max<qint64>(0, m_routingAsset.compressedSize - partial)
                                  + m_routingAsset.size;
                } else {
                    totalNeeded += m_routingAsset.size;
                    const qint64 partial = QFileInfo(routingPartPath()).size();
                    diskNeeded += std::max<qint64>(0, m_routingAsset.size - partial);
                }
            }
        }

        emit estimatesChanged();

        // Both artifacts already match the installed digests - nothing to download.
        if (m_displayDone && m_routingDone) {
            doFinishAll();
            return;
        }

        static constexpr qint64 DiskSpaceHeadroom = 16LL * 1024 * 1024;
        if (!hasEnoughDiskSpace(diskNeeded + DiskSpaceHeadroom)) {
            setError(QStringLiteral("Insufficient disk space"));
            return;
        }

        setStatus(ScootEnums::MapDownloadStatus::Downloading);
        m_totalBytes = totalNeeded;
        m_downloadedBytes = 0;
        m_completedBytes = 0;
        emit progressChanged();

        if (!m_displayDone) {
            doDownloadFile(m_displayAsset.url, displayPartPath(),
                          m_displayAsset.digest, m_displayAsset.size, true);
        } else if (!m_routingDone) {
            if (m_routingAsset.useCompressed()) {
                doDownloadFile(m_routingAsset.compressedUrl, routingCompressedPartPath(),
                              m_routingAsset.compressedDigest, m_routingAsset.compressedSize, false);
            } else {
                doDownloadFile(m_routingAsset.url, routingPartPath(),
                              m_routingAsset.digest, m_routingAsset.size, false);
            }
        }
    });
}

void MapDownloadService::doDownloadFile(const QString &url, const QString &destPath,
                                         const QString &digest, qint64 expectedSize,
                                         bool isDisplay)
{
    QDir().mkpath(downloadDir());

    QNetworkRequest req{QUrl{url}};
    req.setRawHeader("User-Agent", "Librescoot/1.0");

    // Check for partial download (resume)
    qint64 existingSize = 0;
    QFile existing(destPath);
    if (existing.exists()) {
        existingSize = existing.size();
        if (existingSize > 0 && existingSize < expectedSize) {
            req.setRawHeader("Range", QStringLiteral("bytes=%1-").arg(existingSize).toUtf8());
        } else if (existingSize >= expectedSize) {
            // Already fully downloaded, verify
            doVerify(destPath, digest, isDisplay ? displayDestPath() : routingDestPath(), isDisplay);
            return;
        }
    }

    m_currentFile = new QFile(destPath, this);
    QIODevice::OpenMode mode = existingSize > 0 ? QIODevice::Append : QIODevice::WriteOnly;
    m_resumeAppend = existingSize > 0;
    if (!m_currentFile->open(mode)) {
        setError(QStringLiteral("Could not open file for writing"));
        m_currentFile->deleteLater();
        m_currentFile = nullptr;
        return;
    }

    req.setTransferTimeout(30000);
    m_currentReply = m_nam->get(req);

    // m_currentFile->size() already includes any existingSize because the file
    // is opened in Append mode for resumes; m_completedBytes carries the bytes
    // from previously-finished files in this session (e.g. display done, now
    // downloading routing). Together they give the cumulative session bytes.
    connect(m_currentReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_currentFile && m_currentReply) {
            // We opened in Append mode expecting a 206 Partial Content, but some
            // servers ignore the Range header and answer with the full body
            // (200). Left alone that would land on top of the existing partial
            // and corrupt it, so reset the file once, on the first chunk.
            if (m_resumeAppend) {
                int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (statusCode == 200) {
                    m_currentFile->seek(0);
                    m_currentFile->resize(0);
                }
                m_resumeAppend = false;
            }

            QByteArray data = m_currentReply->readAll();
            qint64 written = m_currentFile->write(data);
            if (written != data.size()) {
                setError(QStringLiteral("Could not write map data (disk full?)"));
                if (m_currentReply)
                    m_currentReply->abort();
                return;
            }
            m_downloadedBytes = m_completedBytes + m_currentFile->size();
            m_progress = m_totalBytes > 0 ? static_cast<double>(m_downloadedBytes) / m_totalBytes : 0.0;
            emit progressChanged();
        }
    });

    connect(m_currentReply, &QNetworkReply::finished, this,
            [this, destPath, digest, isDisplay]() {
        auto *reply = m_currentReply;
        m_currentReply = nullptr;

        if (m_currentFile) {
            m_currentFile->close();
            m_currentFile->deleteLater();
            m_currentFile = nullptr;
        }

        if (reply) {
            reply->deleteLater();
            if (m_cancelled) return;
            // Already reported (e.g. the write-error abort above) - don't let
            // the aborted reply's stale HTTP status paper over that error.
            if (m_status == ScootEnums::MapDownloadStatus::Error) return;

            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            // Any error lands here except a user cancel (m_cancelled, returned
            // above) and a write-error abort (Error status, returned above). A
            // transfer-timeout abort surfaces as OperationCanceledError and must
            // be treated as a failure so the .part is preserved for resume,
            // rather than falling through to verify -> SHA mismatch -> delete.
            if (reply->error() != QNetworkReply::NoError) {
                setError(QStringLiteral("Download failed: ") + reply->errorString());
                return;
            }

            // Accept 200 (full) or 206 (partial/resume)
            if (statusCode != 200 && statusCode != 206) {
                setError(QStringLiteral("Download failed with HTTP %1").arg(statusCode));
                return;
            }
        }

        doVerify(destPath, digest, isDisplay ? displayDestPath() : routingDestPath(), isDisplay);
    });
}

void MapDownloadService::doVerify(const QString &filePath, const QString &expectedDigest,
                                    const QString &destPath, bool isDisplay)
{
    if (m_cancelled) return;

    setStatus(ScootEnums::MapDownloadStatus::Installing);

    if (expectedDigest.isEmpty()) {
        doInstall(filePath, destPath, isDisplay);
        return;
    }

    // Run SHA256 verification in background thread
    auto future = QtConcurrent::run([filePath]() -> QString {
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        char buf[65536];
        while (!f.atEnd()) {
            qint64 read = f.read(buf, sizeof(buf));
            if (read > 0)
                hash.addData(QByteArrayView(buf, read));
        }
        return QString::fromLatin1(hash.result().toHex());
    });

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
            [this, watcher, expectedDigest, filePath, destPath, isDisplay]() {
        QString computed = watcher->result();
        watcher->deleteLater();

        if (m_cancelled) return;

        if (computed != expectedDigest) {
            qWarning() << "SHA256 mismatch for" << filePath
                       << "expected:" << expectedDigest << "got:" << computed;
            QFile::remove(filePath);
            setError(QStringLiteral("Download verification failed, please retry"));
            return;
        }

        doInstall(filePath, destPath, isDisplay, computed);
    });
    watcher->setFuture(future);
}

void MapDownloadService::doInstall(const QString &tempPath, const QString &destPath,
                                    bool isDisplay, const QString &digest)
{
    if (m_cancelled) return;

    QDir().mkpath(QFileInfo(destPath).absolutePath());

    // The routing archive is transferred compressed but has to land as a plain
    // seekable tar, because valhalla mmaps it as its tile_extract. Decompress
    // into a sibling .part first so the rename below stays atomic.
    QString installSource = tempPath;
    if (!isDisplay && m_routingAsset.useCompressed()
        && tempPath == routingCompressedPartPath()) {
        const QString decompressedPath = routingPartPath();
        QString err;
        const qint64 total = m_routingAsset.size;
        const bool ok = ZstdDecompressor::decompressFile(
            tempPath, decompressedPath, total,
            [this, total](qint64 done) {
                m_progress = total > 0 ? static_cast<double>(done) / total : 0.0;
                emit progressChanged();
            },
            &err);
        if (!ok) {
            QFile::remove(decompressedPath);
            QFile::remove(tempPath);
            setError(err);
            return;
        }
        QFile::remove(tempPath);
        installSource = decompressedPath;
    }

    // Atomic replace via POSIX rename: downloadDir/mapsDir/valhalla all live on
    // the same filesystem, so this swaps the file in a single step with no
    // window where destPath is missing for a concurrent reader.
    if (::rename(QFile::encodeName(installSource).constData(),
                 QFile::encodeName(destPath).constData()) != 0) {
        setError(QStringLiteral("Could not install maps"));
        return;
    }

    // Update metadata
    MapTileInfo info;
    // Persisted digest must be what checkForUpdates() and the
    // already-installed fast path in doFetchReleases() compare it against:
    // the manifest's uncompressed sha256 (m_routingAsset.digest). On the
    // compressed routing path, `digest` here is the just-verified digest of
    // the .tar.zst (m_routingAsset.compressedDigest), which is not what ends
    // up on disk - persisting that would make every future update check see
    // a permanent, unrepairable mismatch and re-download the whole archive
    // every time. The display asset has no compressed variant, so its
    // verified digest is already the right one to persist; the non-compressed
    // routing path already verifies against m_routingAsset.digest, so this is
    // a no-op there too.
    info.digest = isDisplay ? digest : m_routingAsset.digest;
    info.size = QFileInfo(destPath).size();
    info.publishedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // Roll the just-installed file into the cumulative session counter so
    // the next file's progress starts from where this one ended, not 0.
    m_completedBytes += info.size;

    if (isDisplay) {
        m_metadata.displayTiles = info;
        m_displayDone = true;
    } else {
        m_metadata.valhallaTiles = info;
        m_routingDone = true;

        // Restart valhalla service after installing routing maps
#ifdef Q_OS_LINUX
        QProcess::startDetached(QStringLiteral("systemctl"),
                                {QStringLiteral("restart"), QStringLiteral("valhalla")});
#endif
    }

    m_metadata.region = m_resolvedSlug;
    MapMetadata::save(m_metadata);

    emit partialStateChanged();

    // Check if we need to download the other type
    if (!m_displayDone) {
        // Should not happen in current flow since display is downloaded first
    } else if (!m_routingDone && m_needsRouting) {
        if (m_routingAsset.useCompressed()) {
            doDownloadFile(m_routingAsset.compressedUrl, routingCompressedPartPath(),
                           m_routingAsset.compressedDigest, m_routingAsset.compressedSize, false);
        } else {
            doDownloadFile(m_routingAsset.url, routingPartPath(),
                           m_routingAsset.digest, m_routingAsset.size, false);
        }
        return;
    }

    doFinishAll();
}

void MapDownloadService::doFinishAll()
{
    if (m_updateAvailable) {
        m_updateAvailable = false;
        m_metadata.updateAvailable = false;
        MapMetadata::save(m_metadata);
        emit updateAvailableChanged();
    }
    m_progress = 1.0;
    emit progressChanged();
    setStatus(ScootEnums::MapDownloadStatus::Done);
    emit downloadComplete();
}

// --- Helpers ---

QString MapDownloadService::slugForState(const QString &state) const
{
    return s_stateToSlug.value(state);
}

QString MapDownloadService::displayNameForSlug(const QString &slug) const
{
    if (slug.isEmpty())
        return {};
    if (slug == QLatin1String("berlin_brandenburg"))
        return QStringLiteral("Berlin/Brandenburg");
    if (slug == QLatin1String("niedersachsen"))
        return QStringLiteral("Niedersachsen (incl. Bremen)");
    for (auto it = s_stateToSlug.constBegin(); it != s_stateToSlug.constEnd(); ++it) {
        if (it.value() == slug)
            return it.key();
    }
    return slug;
}

QString MapDownloadService::mapsDir() const
{
#ifdef Q_OS_LINUX
    return QStringLiteral("/data/maps");
#else
    return QDir::homePath() + QStringLiteral("/.local/share/scootui/maps");
#endif
}

QString MapDownloadService::downloadDir() const
{
    return mapsDir() + QStringLiteral("/.download");
}

QString MapDownloadService::displayPartPath() const
{
    return downloadDir() + QStringLiteral("/display.mbtiles.part");
}

QString MapDownloadService::routingPartPath() const
{
    return downloadDir() + QStringLiteral("/routing.tar.part");
}

QString MapDownloadService::routingCompressedPartPath() const
{
    return downloadDir() + QStringLiteral("/routing.tar.zst.part");
}

QString MapDownloadService::displayDestPath() const
{
#ifdef Q_OS_LINUX
    return QStringLiteral("/data/maps/map.mbtiles");
#else
    return mapsDir() + QStringLiteral("/map.mbtiles");
#endif
}

QString MapDownloadService::routingDestPath() const
{
#ifdef Q_OS_LINUX
    return QStringLiteral("/data/valhalla/tiles.tar");
#else
    return mapsDir() + QStringLiteral("/tiles.tar");
#endif
}

bool MapDownloadService::hasEnoughDiskSpace(qint64 needed) const
{
    QDir().mkpath(mapsDir());
    QStorageInfo storage(mapsDir());
    return storage.bytesAvailable() > needed;
}

bool MapDownloadService::hasMapsInstalled() const
{
    return QFile::exists(displayDestPath());
}

void MapDownloadService::adoptInstalledMaps()
{
    bool changed = false;

    // The flasher uploads map.mbtiles and tiles.tar directly and writes no
    // metadata, so on a freshly provisioned vehicle the files are there but
    // this service believes nothing is installed. Record what is on disk;
    // computeMissingDigests() fills in the digests right after.
    if (!m_metadata.displayTiles && QFile::exists(displayDestPath())) {
        MapTileInfo info;
        info.size = QFileInfo(displayDestPath()).size();
        m_metadata.displayTiles = info;
        changed = true;
    }
    if (!m_metadata.valhallaTiles && QFile::exists(routingDestPath())) {
        MapTileInfo info;
        info.size = QFileInfo(routingDestPath()).size();
        m_metadata.valhallaTiles = info;
        changed = true;
    }

    if (changed) {
        qDebug() << "Adopting map files installed outside the dashboard";
        MapMetadata::save(m_metadata);
    }
}

bool MapDownloadService::adoptRegionFromManifest(const QJsonObject &manifest)
{
    const QString displayDigest =
        m_metadata.displayTiles ? m_metadata.displayTiles->digest : QString();
    const QString routingDigest =
        m_metadata.valhallaTiles ? m_metadata.valhallaTiles->digest : QString();
    if (displayDigest.isEmpty() && routingDigest.isEmpty())
        return false;

    for (auto it = manifest.constBegin(); it != manifest.constEnd(); ++it) {
        const QJsonObject region = it.value().toObject();
        const QString mapSha =
            region[QStringLiteral("map")].toObject()[QStringLiteral("sha256")].toString();
        const QString valhallaSha =
            region[QStringLiteral("valhalla")].toObject()[QStringLiteral("sha256")].toString();

        if ((!displayDigest.isEmpty() && displayDigest == mapSha)
            || (!routingDigest.isEmpty() && routingDigest == valhallaSha)) {
            m_resolvedSlug = it.key();
            m_regionName = displayNameForSlug(m_resolvedSlug);
            m_metadata.region = m_resolvedSlug;
            MapMetadata::save(m_metadata);
            emit regionNameChanged();
            qDebug() << "Identified installed region from tile digests:" << m_resolvedSlug;
            return true;
        }
    }
    return false;
}

void MapDownloadService::computeMissingDigests()
{
    struct Job {
        QString filePath;
        bool isDisplay;
    };

    QList<Job> jobs;
    if (m_metadata.displayTiles && m_metadata.displayTiles->digest.isEmpty()
        && QFile::exists(displayDestPath()))
        jobs.append({displayDestPath(), true});
    if (m_metadata.valhallaTiles && m_metadata.valhallaTiles->digest.isEmpty()
        && QFile::exists(routingDestPath()))
        jobs.append({routingDestPath(), false});

    if (jobs.isEmpty())
        return;

    qDebug() << "Computing SHA256 for" << jobs.size() << "installed map file(s)";

    for (const auto &job : jobs) {
        auto future = QtConcurrent::run([path = job.filePath]() -> QString {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly))
                return {};
            QCryptographicHash hash(QCryptographicHash::Sha256);
            char buf[65536];
            while (!f.atEnd()) {
                qint64 read = f.read(buf, sizeof(buf));
                if (read > 0)
                    hash.addData(QByteArrayView(buf, read));
            }
            return QString::fromLatin1(hash.result().toHex());
        });

        auto *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this,
                [this, watcher, isDisplay = job.isDisplay]() {
            QString digest = watcher->result();
            watcher->deleteLater();

            if (digest.isEmpty())
                return;

            if (isDisplay && m_metadata.displayTiles) {
                m_metadata.displayTiles->digest = digest;
                qDebug() << "Computed display map digest:" << digest;
            } else if (!isDisplay && m_metadata.valhallaTiles) {
                m_metadata.valhallaTiles->digest = digest;
                qDebug() << "Computed routing map digest:" << digest;
            }
            MapMetadata::save(m_metadata);
        });
        watcher->setFuture(future);
    }
}

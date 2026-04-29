#include "MapDownloadService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QStorageInfo>
#include <QDebug>
#include <QtConcurrent>

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
    return QFile::exists(routingPartPath());
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

void MapDownloadService::checkForUpdates()
{
    if (m_resolvedSlug.isEmpty())
        return;

    if (m_status != ScootEnums::MapDownloadStatus::Idle &&
        m_status != ScootEnums::MapDownloadStatus::Error)
        return;

    setStatus(ScootEnums::MapDownloadStatus::CheckingUpdates);

    fetchTilesManifest([this](const QJsonObject &manifest) {
        if (manifest.isEmpty()) {
            setStatus(ScootEnums::MapDownloadStatus::Idle);
            return;
        }

        auto region = manifest[m_resolvedSlug].toObject();
        if (region.isEmpty()) {
            setStatus(ScootEnums::MapDownloadStatus::Idle);
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

        if (hasUpdate != m_updateAvailable) {
            m_updateAvailable = hasUpdate;
            m_metadata.updateAvailable = hasUpdate;
            MapMetadata::save(m_metadata);
            emit updateAvailableChanged();
        }
        setStatus(ScootEnums::MapDownloadStatus::Idle);
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
    req.setRawHeader("User-Agent", "LibreScoot/1.0");
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
    req.setRawHeader("User-Agent", "LibreScoot/1.0");
    // s_stateToSlug only knows German state names; force Nominatim to return
    // those regardless of the device's system locale.
    req.setRawHeader("Accept-Language", "de");
    req.setTransferTimeout(10000);

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
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

        qint64 totalNeeded = 0;

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
            totalNeeded += m_displayAsset.size;
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
            totalNeeded += m_routingAsset.size;
        }

        emit estimatesChanged();

        if (!hasEnoughDiskSpace(totalNeeded)) {
            setError(QStringLiteral("Insufficient disk space"));
            return;
        }

        setStatus(ScootEnums::MapDownloadStatus::Downloading);
        m_totalBytes = totalNeeded;
        m_downloadedBytes = 0;
        m_completedBytes = 0;
        emit progressChanged();

        if (needsDisplay) {
            doDownloadFile(m_displayAsset.url, displayPartPath(),
                          m_displayAsset.digest, m_displayAsset.size, true);
        } else if (needsRouting) {
            doDownloadFile(m_routingAsset.url, routingPartPath(),
                          m_routingAsset.digest, m_routingAsset.size, false);
        }
    });
}

void MapDownloadService::doDownloadFile(const QString &url, const QString &destPath,
                                         const QString &digest, qint64 expectedSize,
                                         bool isDisplay)
{
    QDir().mkpath(downloadDir());

    QNetworkRequest req{QUrl{url}};
    req.setRawHeader("User-Agent", "LibreScoot/1.0");

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
    if (!m_currentFile->open(mode)) {
        setError(QStringLiteral("Could not open file for writing"));
        m_currentFile->deleteLater();
        m_currentFile = nullptr;
        return;
    }

    m_currentReply = m_nam->get(req);

    // m_currentFile->size() already includes any existingSize because the file
    // is opened in Append mode for resumes; m_completedBytes carries the bytes
    // from previously-finished files in this session (e.g. display done, now
    // downloading routing). Together they give the cumulative session bytes.
    connect(m_currentReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_currentFile && m_currentReply) {
            QByteArray data = m_currentReply->readAll();
            m_currentFile->write(data);
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

            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError &&
                reply->error() != QNetworkReply::OperationCanceledError) {
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

    // Move file to final destination
    QFile::remove(destPath);
    if (!QFile::rename(tempPath, destPath)) {
        setError(QStringLiteral("Could not install maps"));
        return;
    }

    // Update metadata
    MapTileInfo info;
    info.digest = digest;
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
        doDownloadFile(m_routingAsset.url, routingPartPath(),
                       m_routingAsset.digest, m_routingAsset.size, false);
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

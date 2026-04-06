#include "MapDownloadService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
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
    s_instance = this;
    m_metadata = MapMetadata::load();
    if (!m_metadata.region.isEmpty()) {
        m_resolvedSlug = m_metadata.region;
    }

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
    emit progressChanged();
    setStatus(ScootEnums::MapDownloadStatus::Idle);
}

void MapDownloadService::checkForUpdates()
{
    if (m_resolvedSlug.isEmpty())
        return;

    setStatus(ScootEnums::MapDownloadStatus::CheckingUpdates);

    // Fetch releases and compare digests with metadata
    QUrl apiUrl{QStringLiteral("https://api.github.com/repos/librescoot/osm-tiles/releases/tags/latest")};
    QNetworkRequest req{apiUrl};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(15000);

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(ScootEnums::MapDownloadStatus::Idle);
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto assets = doc.object()[QStringLiteral("assets")].toArray();

        QString displayName = QStringLiteral("tiles_") + m_resolvedSlug + QStringLiteral(".mbtiles");
        QString routingName = QStringLiteral("valhalla_tiles_") + m_resolvedSlug + QStringLiteral(".tar");

        bool hasUpdate = false;
        for (const auto &a : assets) {
            auto obj = a.toObject();
            QString name = obj[QStringLiteral("name")].toString();
            if (name == displayName && m_metadata.displayTiles) {
                // Check if size differs (simple update detection)
                qint64 remoteSize = obj[QStringLiteral("size")].toDouble();
                if (remoteSize != m_metadata.displayTiles->size)
                    hasUpdate = true;
            }
            if (name == routingName && m_metadata.valhallaTiles) {
                qint64 remoteSize = obj[QStringLiteral("size")].toDouble();
                if (remoteSize != m_metadata.valhallaTiles->size)
                    hasUpdate = true;
            }
        }

        if (hasUpdate != m_updateAvailable) {
            m_updateAvailable = hasUpdate;
            emit updateAvailableChanged();
        }
        setStatus(ScootEnums::MapDownloadStatus::Idle);
    });
}

// --- Pipeline stages ---

void MapDownloadService::doResolveSlug(double lat, double lng)
{
    QString url = QStringLiteral("https://nominatim.openstreetmap.org/reverse?lat=%1&lon=%2&format=json&zoom=5")
                      .arg(lat, 0, 'f', 6).arg(lng, 0, 'f', 6);

    QNetworkRequest req{QUrl{url}};
    req.setRawHeader("User-Agent", "LibreScoot/1.0");
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
        // Use friendly display name for combined regions
        if (slug == QLatin1String("berlin_brandenburg"))
            m_regionName = QStringLiteral("Berlin/Brandenburg");
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
    // Fetch display tile size from osm-tiles repo
    QUrl displayUrl{QStringLiteral("https://api.github.com/repos/librescoot/osm-tiles/releases/tags/latest")};
    QNetworkRequest displayReq{displayUrl};
    displayReq.setRawHeader("Accept", "application/vnd.github+json");
    displayReq.setTransferTimeout(15000);

    auto *displayReply = m_nam->get(displayReq);
    connect(displayReply, &QNetworkReply::finished, this, [this, displayReply]() {
        displayReply->deleteLater();
        if (m_cancelled) return;

        if (displayReply->error() == QNetworkReply::NoError) {
            auto doc = QJsonDocument::fromJson(displayReply->readAll());
            auto assets = doc.object()[QStringLiteral("assets")].toArray();
            QString target = QStringLiteral("tiles_") + m_resolvedSlug + QStringLiteral(".mbtiles");
            for (const auto &a : assets) {
                auto obj = a.toObject();
                if (obj[QStringLiteral("name")].toString() == target) {
                    m_estimatedDisplayBytes = static_cast<qint64>(obj[QStringLiteral("size")].toDouble());
                    break;
                }
            }
        }
        emit estimatesChanged();
    });

    // Fetch routing tile size from valhalla-tiles repo
    QUrl routingUrl{QStringLiteral("https://api.github.com/repos/librescoot/valhalla-tiles/releases/tags/latest")};
    QNetworkRequest routingReq{routingUrl};
    routingReq.setRawHeader("Accept", "application/vnd.github+json");
    routingReq.setTransferTimeout(15000);

    auto *routingReply = m_nam->get(routingReq);
    connect(routingReply, &QNetworkReply::finished, this, [this, routingReply]() {
        routingReply->deleteLater();
        if (m_cancelled) return;

        if (routingReply->error() == QNetworkReply::NoError) {
            auto doc = QJsonDocument::fromJson(routingReply->readAll());
            auto assets = doc.object()[QStringLiteral("assets")].toArray();
            QString target = QStringLiteral("valhalla_tiles_") + m_resolvedSlug + QStringLiteral(".tar");
            for (const auto &a : assets) {
                auto obj = a.toObject();
                if (obj[QStringLiteral("name")].toString() == target) {
                    m_estimatedRoutingBytes = static_cast<qint64>(obj[QStringLiteral("size")].toDouble());
                    break;
                }
            }
        }
        emit estimatesChanged();
        setStatus(ScootEnums::MapDownloadStatus::Idle);
    });
}

void MapDownloadService::doFetchReleases(bool needsDisplay, bool needsRouting)
{
    // Helper to parse asset info from a release response
    auto parseAssets = [](const QByteArray &data) {
        QHash<QString, QJsonObject> map;
        auto doc = QJsonDocument::fromJson(data);
        for (const auto &a : doc.object()[QStringLiteral("assets")].toArray()) {
            auto obj = a.toObject();
            map[obj[QStringLiteral("name")].toString()] = obj;
        }
        return map;
    };

    // Fetch display tiles from osm-tiles repo
    QUrl displayUrl{QStringLiteral("https://api.github.com/repos/librescoot/osm-tiles/releases/tags/latest")};
    QNetworkRequest displayReq{displayUrl};
    displayReq.setRawHeader("Accept", "application/vnd.github+json");
    displayReq.setTransferTimeout(15000);

    auto *displayReply = m_nam->get(displayReq);

    // Fetch routing tiles from valhalla-tiles repo (in parallel)
    QUrl routingUrl{QStringLiteral("https://api.github.com/repos/librescoot/valhalla-tiles/releases/tags/latest")};
    QNetworkRequest routingReq{routingUrl};
    routingReq.setRawHeader("Accept", "application/vnd.github+json");
    routingReq.setTransferTimeout(15000);

    auto *routingReply = needsRouting ? m_nam->get(routingReq) : nullptr;

    // Track completion of both requests
    auto *pending = new int(needsRouting ? 2 : 1);
    auto *displayData = new QByteArray();
    auto *routingData = new QByteArray();

    auto finalize = [this, needsDisplay, needsRouting, pending, displayData, routingData, parseAssets]() {
        if (--(*pending) > 0) return; // Wait for both

        QString sha256Suffix = QStringLiteral(".sha256");
        qint64 totalNeeded = 0;

        if (needsDisplay) {
            auto assetMap = parseAssets(*displayData);
            QString name = QStringLiteral("tiles_") + m_resolvedSlug + QStringLiteral(".mbtiles");
            if (!assetMap.contains(name)) {
                setError(QStringLiteral("Display maps not available for ") + m_regionName);
                delete pending; delete displayData; delete routingData;
                return;
            }
            auto &asset = assetMap[name];
            m_displayAsset.url = asset[QStringLiteral("browser_download_url")].toString();
            m_displayAsset.size = static_cast<qint64>(asset[QStringLiteral("size")].toDouble());
            m_estimatedDisplayBytes = m_displayAsset.size;
            QString sha256Name = name + sha256Suffix;
            if (assetMap.contains(sha256Name))
                m_displayAsset.digest = assetMap[sha256Name][QStringLiteral("browser_download_url")].toString();
            totalNeeded += m_displayAsset.size;
        }

        if (needsRouting) {
            auto assetMap = parseAssets(*routingData);
            QString name = QStringLiteral("valhalla_tiles_") + m_resolvedSlug + QStringLiteral(".tar");
            if (!assetMap.contains(name)) {
                setError(QStringLiteral("Routing maps not available for ") + m_regionName);
                delete pending; delete displayData; delete routingData;
                return;
            }
            auto &asset = assetMap[name];
            m_routingAsset.url = asset[QStringLiteral("browser_download_url")].toString();
            m_routingAsset.size = static_cast<qint64>(asset[QStringLiteral("size")].toDouble());
            m_estimatedRoutingBytes = m_routingAsset.size;
            QString sha256Name = name + sha256Suffix;
            if (assetMap.contains(sha256Name))
                m_routingAsset.digest = assetMap[sha256Name][QStringLiteral("browser_download_url")].toString();
            totalNeeded += m_routingAsset.size;
        }

        delete pending; delete displayData; delete routingData;
        emit estimatesChanged();

        if (!hasEnoughDiskSpace(totalNeeded)) {
            setError(QStringLiteral("Insufficient disk space"));
            return;
        }

        setStatus(ScootEnums::MapDownloadStatus::Downloading);
        m_totalBytes = totalNeeded;
        m_downloadedBytes = 0;
        emit progressChanged();

        if (needsDisplay) {
            doDownloadFile(m_displayAsset.url, displayPartPath(),
                          m_displayAsset.digest, m_displayAsset.size, true);
        } else if (needsRouting) {
            doDownloadFile(m_routingAsset.url, routingPartPath(),
                          m_routingAsset.digest, m_routingAsset.size, false);
        }
    };

    connect(displayReply, &QNetworkReply::finished, this, [this, displayReply, displayData, finalize]() {
        displayReply->deleteLater();
        if (m_cancelled) return;
        if (displayReply->error() != QNetworkReply::NoError) {
            setError(QStringLiteral("Could not fetch display tile info"));
            return;
        }
        *displayData = displayReply->readAll();
        finalize();
    });

    if (routingReply) {
        connect(routingReply, &QNetworkReply::finished, this, [this, routingReply, routingData, finalize]() {
            routingReply->deleteLater();
            if (m_cancelled) return;
            if (routingReply->error() != QNetworkReply::NoError) {
                setError(QStringLiteral("Could not fetch routing tile info"));
                return;
            }
            *routingData = routingReply->readAll();
            finalize();
        });
    }
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

    connect(m_currentReply, &QNetworkReply::readyRead, this, [this, existingSize]() {
        if (m_currentFile && m_currentReply) {
            QByteArray data = m_currentReply->readAll();
            m_currentFile->write(data);
            m_downloadedBytes = existingSize + m_currentFile->size();
            m_progress = m_totalBytes > 0 ? static_cast<double>(m_downloadedBytes) / m_totalBytes : 0.0;
            emit progressChanged();
        }
    });

    connect(m_currentReply, &QNetworkReply::finished, this,
            [this, destPath, digest, isDisplay, existingSize]() {
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

        // If digest is a URL (sha256 file), fetch it first
        if (digest.startsWith(QStringLiteral("http"))) {
            QUrl shaUrl{digest};
            QNetworkRequest shaReq{shaUrl};
            shaReq.setRawHeader("User-Agent", "LibreScoot/1.0");
            auto *shaReplyPtr = m_nam->get(shaReq);
            connect(shaReplyPtr, &QNetworkReply::finished, this,
                    [this, shaReplyPtr, destPath, isDisplay]() {
                shaReplyPtr->deleteLater();
                QString sha256;
                if (shaReplyPtr->error() == QNetworkReply::NoError) {
                    // Format: "<hash>  <filename>"
                    QString content = QString::fromUtf8(shaReplyPtr->readAll()).trimmed();
                    sha256 = content.split(QRegularExpression(QStringLiteral("\\s+"))).first();
                }
                doVerify(destPath, sha256, isDisplay ? displayDestPath() : routingDestPath(), isDisplay);
            });
        } else {
            doVerify(destPath, digest, isDisplay ? displayDestPath() : routingDestPath(), isDisplay);
        }
    });
}

void MapDownloadService::doVerify(const QString &filePath, const QString &expectedDigest,
                                    const QString &destPath, bool isDisplay)
{
    if (m_cancelled) return;

    setStatus(ScootEnums::MapDownloadStatus::Installing);

    if (expectedDigest.isEmpty()) {
        // No digest to verify, just install
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
            // Remove the corrupted file and retry
            QFile::remove(filePath);
            setError(QStringLiteral("Download verification failed, please retry"));
            return;
        }

        doInstall(filePath, destPath, isDisplay);
    });
    watcher->setFuture(future);
}

void MapDownloadService::doInstall(const QString &tempPath, const QString &destPath, bool isDisplay)
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
    info.size = QFileInfo(destPath).size();
    info.publishedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

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

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <functional>
#include "models/Enums.h"
#include "models/MapMetadata.h"
#include "utils/ZstdDecompressor.h"

class MapDownloadService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString regionName READ regionName NOTIFY regionNameChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(qint64 downloadedBytes READ downloadedBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(bool hasPartialDisplayDownload READ hasPartialDisplayDownload NOTIFY partialStateChanged)
    Q_PROPERTY(bool hasPartialRoutingDownload READ hasPartialRoutingDownload NOTIFY partialStateChanged)
    Q_PROPERTY(qint64 estimatedDisplayBytes READ estimatedDisplayBytes NOTIFY estimatesChanged)
    Q_PROPERTY(qint64 estimatedRoutingBytes READ estimatedRoutingBytes NOTIFY estimatesChanged)

public:
    explicit MapDownloadService(QObject *parent = nullptr);

    int status() const { return static_cast<int>(m_status); }
    double progress() const { return m_progress; }
    QString regionName() const { return m_regionName; }
    QString errorMessage() const { return m_errorMessage; }
    bool updateAvailable() const { return m_updateAvailable; }
    qint64 downloadedBytes() const { return m_downloadedBytes; }
    qint64 totalBytes() const { return m_totalBytes; }
    bool hasPartialDisplayDownload() const;
    bool hasPartialRoutingDownload() const;
    qint64 estimatedDisplayBytes() const { return m_estimatedDisplayBytes; }
    qint64 estimatedRoutingBytes() const { return m_estimatedRoutingBytes; }

    Q_INVOKABLE void resolveRegion(double lat, double lng);
    Q_INVOKABLE void startDownload(double lat, double lng, bool needsDisplay, bool needsRouting);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void checkForUpdates();
    // Same, but resolves the region from a GPS fix first when it isn't known
    // yet. Maps installed by the flasher come with no metadata at all, so
    // without this an update check has nothing to look up in the manifest.
    Q_INVOKABLE void checkForUpdatesAt(double lat, double lng);
    // Check using whatever the service can work out on its own: the known
    // region, else a digest match, else the position provider's fix.
    Q_INVOKABLE void checkForUpdatesNow();

    // Supplies a GPS fix when one is available. Returns false when there is
    // none, in which case the check falls back to identifying the region by
    // tile digest.
    void setPositionProvider(std::function<bool(double &, double &)> fn) {
        m_positionProvider = std::move(fn);
    }

    // Files on disk, not bookkeeping: maps installed by the flasher are just as
    // installed as ones this service downloaded.
    bool hasMapsInstalled() const;
    bool hasResolvedRegion() const { return !m_resolvedSlug.isEmpty(); }
    // Re-read /data/maps/metadata.json. The constructor runs before /data is
    // mounted, so its read always comes back empty; call this once the
    // partition is available. No-op while a download or check is in flight.
    void reloadMetadata();
    // ISO-8601 UTC, empty when this vehicle has never completed a check.
    QString lastUpdateCheck() const { return m_metadata.lastUpdateCheck; }
    bool shouldCheckForUpdates() const;

signals:
    void statusChanged();
    void progressChanged();
    void regionNameChanged();
    void errorMessageChanged();
    void updateAvailableChanged();
    void partialStateChanged();
    void estimatesChanged();
    void downloadComplete();
    // Fires once per completed update check, whether or not anything changed,
    // so a manual check can report back even when there is nothing to report.
    void updateCheckCompleted(bool updateFound);

private:
    void setStatus(ScootEnums::MapDownloadStatus s);
    void setError(const QString &msg);

    // Pipeline stages
    void doResolveSlug(double lat, double lng);
    void fetchEstimates();
    void doFetchReleases(bool needsDisplay, bool needsRouting);
    void doDownloadFile(const QString &url, const QString &destPath, const QString &digest,
                        qint64 expectedSize, bool isDisplay);
    void doVerify(const QString &filePath, const QString &expectedDigest,
                  const QString &destPath, bool isDisplay);
    void doInstall(const QString &tempPath, const QString &destPath, bool isDisplay,
                   const QString &digest = {});
    // Decompression of a routing archive, off the GUI thread. Resumes in
    // finishInstall() once the worker is done.
    void startDecompressInstall(const QString &compressedPath, const QString &destPath,
                                const QString &digest);
    // Tail of doInstall: the atomic rename plus the metadata bookkeeping that
    // follows it. Always runs on the thread that owns this object.
    void finishInstall(const QString &installSource, const QString &destPath, bool isDisplay,
                       const QString &digest);
    // Picks the compressed or the plain transfer and drops the other path's
    // leftover .part. Both places that begin the routing download go through here.
    void startRoutingDownload();
    void doFinishAll();

    void fetchTilesManifest(std::function<void(const QJsonObject &)> callback);

    void computeMissingDigests();
    // Record map files that are on disk but absent from metadata.json, so their
    // digests get computed and later update checks have something to compare.
    void adoptInstalledMaps();
    // Identify the installed region by matching our digests against every
    // region in the manifest. Works whenever the installed tiles are still the
    // published ones, which is the case right after provisioning.
    bool adoptRegionFromManifest(const QJsonObject &manifest);

    // Helpers
    QString slugForState(const QString &state) const;
    QString displayNameForSlug(const QString &slug) const;
    QString mapsDir() const;
    QString downloadDir() const;
    QString displayPartPath() const;
    QString routingPartPath() const;
    QString routingCompressedPartPath() const;
    QString displayDestPath() const;
    QString routingDestPath() const;
    bool hasEnoughDiskSpace(qint64 needed) const;

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_currentReply = nullptr;
    QFile *m_currentFile = nullptr;
    bool m_cancelled = false;
    // Set while an update check is waiting on region resolution to finish.
    bool m_pendingUpdateCheck = false;
    std::function<bool(double &, double &)> m_positionProvider;
    // Set when the current file was opened in Append mode to resume a partial
    // download. Cleared after the first readyRead chunk of the reply has been
    // checked for a server that ignored our Range header (see doDownloadFile).
    bool m_resumeAppend = false;
    // Non-null only while a routing archive is being decompressed on a worker
    // thread. cancel() uses it to ask that worker to stop.
    QFutureWatcher<ZstdDecompressor::Outcome> *m_decompressWatcher = nullptr;

    ScootEnums::MapDownloadStatus m_status = ScootEnums::MapDownloadStatus::Idle;
    double m_progress = 0.0;
    QString m_regionName;
    QString m_errorMessage;
    bool m_updateAvailable = false;
    qint64 m_downloadedBytes = 0;
    qint64 m_totalBytes = 0;
    // Sum of fully-installed file sizes in the current download session.
    // Carried across files so multi-pack downloads don't reset to 0% mid-way.
    qint64 m_completedBytes = 0;
    qint64 m_estimatedDisplayBytes = 0;
    qint64 m_estimatedRoutingBytes = 0;

    // Current download state
    QString m_resolvedSlug;
    MapMetadata m_metadata;

    // Track what's been requested and what's done
    bool m_needsDisplay = false;
    bool m_needsRouting = false;
    bool m_displayDone = false;
    bool m_routingDone = false;

    // GitHub release info
    struct AssetInfo {
        // Always the uncompressed artifact: what ends up on disk.
        QString url;
        QString digest;
        qint64 size = 0;
        // Set only when the manifest offers a compressed variant in a codec
        // this build understands. When set, the download fetches these and
        // decompresses to `size` bytes at install time.
        QString compressedUrl;
        QString compressedDigest;
        qint64 compressedSize = 0;

        bool useCompressed() const { return !compressedUrl.isEmpty(); }
    };
    AssetInfo m_displayAsset;
    AssetInfo m_routingAsset;

    // sha256 of a compressed routing archive that arrived intact and still
    // would not decode. Set for the rest of the session so doFetchReleases()
    // can decline the same artifact when the manifest offers it again; a newly
    // published one has a different digest and is tried normally.
    QString m_rejectedCompressedDigest;

    static const QHash<QString, QString> s_stateToSlug;
};

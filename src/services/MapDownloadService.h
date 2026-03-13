#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QCryptographicHash>
#include "models/Enums.h"
#include "models/MapMetadata.h"

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
    explicit MapDownloadService(bool simulatorMode, QObject *parent = nullptr);

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

signals:
    void statusChanged();
    void progressChanged();
    void regionNameChanged();
    void errorMessageChanged();
    void updateAvailableChanged();
    void partialStateChanged();
    void estimatesChanged();
    void downloadComplete();

private:
    void setStatus(ScootEnums::MapDownloadStatus s);
    void setError(const QString &msg);

    // Pipeline stages
    void doResolveSlug(double lat, double lng);
    void doFetchReleases(bool needsDisplay, bool needsRouting);
    void doDownloadFile(const QString &url, const QString &destPath, const QString &digest,
                        qint64 expectedSize, bool isDisplay);
    void doVerify(const QString &filePath, const QString &expectedDigest,
                  const QString &destPath, bool isDisplay);
    void doInstall(const QString &tempPath, const QString &destPath, bool isDisplay);
    void doFinishAll();

    // Helpers
    QString slugForState(const QString &state) const;
    QString mapsDir() const;
    QString downloadDir() const;
    QString displayPartPath() const;
    QString routingPartPath() const;
    QString displayDestPath() const;
    QString routingDestPath() const;
    bool hasEnoughDiskSpace(qint64 needed) const;

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_currentReply = nullptr;
    QFile *m_currentFile = nullptr;
    bool m_simulatorMode;
    bool m_cancelled = false;

    ScootEnums::MapDownloadStatus m_status = ScootEnums::MapDownloadStatus::Idle;
    double m_progress = 0.0;
    QString m_regionName;
    QString m_errorMessage;
    bool m_updateAvailable = false;
    qint64 m_downloadedBytes = 0;
    qint64 m_totalBytes = 0;
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
        QString url;
        QString digest;
        qint64 size = 0;
    };
    AssetInfo m_displayAsset;
    AssetInfo m_routingAsset;

    static const QHash<QString, QString> s_stateToSlug;
};

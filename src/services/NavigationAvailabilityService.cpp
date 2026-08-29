#include "NavigationAvailabilityService.h"
#include "stores/SettingsStore.h"
#include "stores/InternetStore.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QDebug>
#include "repositories/RedisSchema.h"

NavigationAvailabilityService::NavigationAvailabilityService(SettingsStore *settings,
                                                               InternetStore *internet,
                                                               MdbRepository *repo,
                                                               QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_internet(internet)
    , m_repo(repo)
    , m_nam(new QNetworkAccessManager(this))
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &NavigationAvailabilityService::checkRouting);

    m_mapsRetryTimer.setSingleShot(true);
    connect(&m_mapsRetryTimer, &QTimer::timeout, this, &NavigationAvailabilityService::checkMaps);

    connect(m_settings, &SettingsStore::valhallaUrlChanged, this, &NavigationAvailabilityService::recheck);
    connect(m_internet, &InternetStore::modemStateChanged, this, &NavigationAvailabilityService::recheck);

    recheck();
}

void NavigationAvailabilityService::recheck()
{
    if (m_overrideActive) return;
    // External trigger (settings change, modem up): reset backoff so we
    // don't sit on a 30s retry when the environment just changed.
    m_retryTimer.stop();
    m_retryDelayMs = 1000;
    m_mapsRetryTimer.stop();
    m_mapsRetryDelayMs = 1000;
    checkMaps();
    checkRouting();
}

void NavigationAvailabilityService::setOverride(bool maps, bool routing)
{
    m_overrideActive = true;
    m_retryTimer.stop();
    m_mapsRetryTimer.stop();
    bool changed = false;
    bool mapsBecameAvailable = maps && !m_mapsAvailable;
    if (maps != m_mapsAvailable) {
        m_mapsAvailable = maps;
        changed = true;
    }
    if (routing != m_routingAvailable) {
        m_routingAvailable = routing;
        changed = true;
    }
    publishToRedis();
    if (changed) emit availabilityChanged();
    if (mapsBecameAvailable) emit localMapsBecameAvailable();
}

void NavigationAvailabilityService::clearOverride()
{
    if (!m_overrideActive) return;
    m_overrideActive = false;
    recheck();
}

void NavigationAvailabilityService::checkMaps()
{
    if (m_overrideActive) return;
    // Check local directory first (desktop/simulator), then device path
    bool available = QFile::exists(QStringLiteral("map.mbtiles"))
                  || QFile::exists(QStringLiteral("/data/maps/map.mbtiles"));
    if (available != m_mapsAvailable) {
        bool becameAvailable = available && !m_mapsAvailable;
        m_mapsAvailable = available;
        publishToRedis();
        emit availabilityChanged();
        // The map/road-info services load the mbtiles once and don't self-poll
        // for a late mount; drive their reload off this edge so they recover
        // when the poller finally sees /data mounted.
        if (becameAvailable)
            emit localMapsBecameAvailable();
    }
    if (!available) {
        // Poll until the tiles show up. The QFileSystemWatcher fallback in
        // Application can't catch /data being mounted over the watched
        // mountpoint (inotify delivers no event for a mount), so a one-shot
        // check latches false when scootui starts before /data is mounted.
        m_mapsRetryTimer.start(m_mapsRetryDelayMs);
        m_mapsRetryDelayMs = qMin(m_mapsRetryDelayMs * 2, 10000);
    } else {
        m_mapsRetryTimer.stop();
        m_mapsRetryDelayMs = 1000;
    }
}

void NavigationAvailabilityService::checkRouting()
{
    if (m_overrideActive) return;
    QString url = m_settings->valhallaUrl();
    if (url.isEmpty())
        url = QLatin1String(AppConfig::valhallaOnDeviceEndpoint);

    QNetworkRequest req(QUrl(url + QStringLiteral("status")));
    req.setTransferTimeout(5000);

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        bool available = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        if (available != m_routingAvailable) {
            m_routingAvailable = available;
            publishToRedis();
            emit availabilityChanged();
        }
        if (!available) {
            scheduleRetry();
        } else {
            m_retryTimer.stop();
            m_retryDelayMs = 1000;
        }
    });
}

void NavigationAvailabilityService::scheduleRetry()
{
    if (m_retryTimer.isActive())
        return;
    m_retryTimer.start(m_retryDelayMs);
    // Exponential backoff: 1s → 2s → 4s → … capped at 30s so a long-down
    // valhalla doesn't keep spinning at 5 Hz, while a startup race still
    // recovers within a second or two.
    m_retryDelayMs = qMin(m_retryDelayMs * 2, 30000);
}

void NavigationAvailabilityService::publishToRedis()
{
    if (!m_repo)
        return;

    const auto &cluster = RedisSchema::hash::Dashboard;
    m_repo->set(cluster, QStringLiteral("maps-available"),
                m_mapsAvailable ? QStringLiteral("true") : QStringLiteral("false"));
    m_repo->set(cluster, QStringLiteral("navigation-available"),
                m_routingAvailable ? QStringLiteral("true") : QStringLiteral("false"));
}

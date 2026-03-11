#include "NavigationAvailabilityService.h"
#include "stores/SettingsStore.h"
#include "stores/InternetStore.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QFile>
#include <QNetworkReply>
#include <QDebug>

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
    connect(m_settings, &SettingsStore::valhallaUrlChanged, this, &NavigationAvailabilityService::recheck);
    connect(m_internet, &InternetStore::modemStateChanged, this, &NavigationAvailabilityService::recheck);

    recheck();
}

void NavigationAvailabilityService::recheck()
{
    checkMaps();
    checkRouting();
}

void NavigationAvailabilityService::checkMaps()
{
    bool available = QFile::exists(QStringLiteral("/data/maps/map.mbtiles"));
    if (available != m_mapsAvailable) {
        m_mapsAvailable = available;
        publishToRedis();
        emit availabilityChanged();
    }
}

void NavigationAvailabilityService::checkRouting()
{
    QString url = m_settings->valhallaUrl();
    if (url.isEmpty())
        url = QLatin1String(AppConfig::valhallaOnDeviceEndpoint);

    QNetworkRequest req(QUrl(url + QStringLiteral("status")));
    req.setTransferTimeout(3000);

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        bool available = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        if (available != m_routingAvailable) {
            m_routingAvailable = available;
            publishToRedis();
            emit availabilityChanged();
        }
    });
}

void NavigationAvailabilityService::publishToRedis()
{
    if (!m_repo)
        return;

    const auto cluster = QStringLiteral("settings");
    m_repo->set(cluster, QStringLiteral("maps-available"),
                m_mapsAvailable ? QStringLiteral("true") : QStringLiteral("false"));
    m_repo->set(cluster, QStringLiteral("navigation-available"),
                m_routingAvailable ? QStringLiteral("true") : QStringLiteral("false"));
}

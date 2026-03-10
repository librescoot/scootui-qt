#include "NavigationAvailabilityService.h"
#include "stores/SettingsStore.h"
#include "stores/InternetStore.h"
#include "core/AppConfig.h"

#include <QFile>
#include <QNetworkReply>
#include <QDebug>

NavigationAvailabilityService::NavigationAvailabilityService(SettingsStore *settings,
                                                               InternetStore *internet,
                                                               QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_internet(internet)
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
    bool available = QFile::exists(QStringLiteral("/data/scootui/maps/map.mbtiles"));
    if (available != m_mapsAvailable) {
        m_mapsAvailable = available;
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
            emit availabilityChanged();
        }
    });
}

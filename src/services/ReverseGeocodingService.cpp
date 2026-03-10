#include "ReverseGeocodingService.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QDebug>

ReverseGeocodingService::ReverseGeocodingService(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void ReverseGeocodingService::resolve(double latitude, double longitude)
{
    QUrl url(QStringLiteral("https://nominatim.openstreetmap.org/reverse"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("lat"), QString::number(latitude, 'f', 7));
    query.addQueryItem(QStringLiteral("lon"), QString::number(longitude, 'f', 7));
    query.addQueryItem(QStringLiteral("zoom"), QStringLiteral("18"));
    query.addQueryItem(QStringLiteral("addressdetails"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("accept-language"), QStringLiteral("en-US,en"));
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setTransferTimeout(10000);
    req.setRawHeader("User-Agent", "ScootUI/1.0");

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit resolveFailed(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        QJsonObject addr = root[QStringLiteral("address")].toObject();

        QStringList parts;

        // Street + house number
        QString road = addr[QStringLiteral("road")].toString();
        QString houseNumber = addr[QStringLiteral("house_number")].toString();
        if (!road.isEmpty()) {
            parts.append(houseNumber.isEmpty() ? road : road + QStringLiteral(" ") + houseNumber);
        }

        // City/town/village + postal code
        QString city = addr[QStringLiteral("city")].toString();
        if (city.isEmpty()) city = addr[QStringLiteral("town")].toString();
        if (city.isEmpty()) city = addr[QStringLiteral("village")].toString();
        QString postcode = addr[QStringLiteral("postcode")].toString();
        if (!city.isEmpty()) {
            parts.append(postcode.isEmpty() ? city : postcode + QStringLiteral(" ") + city);
        }

        // Country
        QString country = addr[QStringLiteral("country")].toString();
        if (!country.isEmpty())
            parts.append(country);

        QString address = parts.isEmpty() ? root[QStringLiteral("display_name")].toString() : parts.join(QStringLiteral(", "));
        emit addressResolved(address);
    });
}

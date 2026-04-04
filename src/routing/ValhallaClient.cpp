#include "ValhallaClient.h"
#include "RouteHelpers.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration>
#include <QDebug>

ValhallaClient::ValhallaClient(QObject *parent)
    : QObject(parent)
    , m_endpoint(QStringLiteral("http://127.0.0.1:8002/"))
{
}

void ValhallaClient::setEndpoint(const QString &url)
{
    m_endpoint = url;
    if (!m_endpoint.endsWith(QLatin1Char('/')))
        m_endpoint.append(QLatin1Char('/'));
}

void ValhallaClient::setLanguage(const QString &lang)
{
    m_language = lang;
    qDebug() << "ValhallaClient: language set to" << m_language;
}

void ValhallaClient::calculateRoute(const LatLng &from, const LatLng &to)
{
    QJsonObject request;
    QJsonArray locations;
    locations.append(QJsonObject{{QStringLiteral("lat"), from.latitude},
                                  {QStringLiteral("lon"), from.longitude},
                                  {QStringLiteral("radius"), 150}});
    locations.append(QJsonObject{{QStringLiteral("lat"), to.latitude},
                                  {QStringLiteral("lon"), to.longitude},
                                  {QStringLiteral("radius"), 150}});
    request[QStringLiteral("locations")] = locations;
    request[QStringLiteral("costing")] = QStringLiteral("motor_scooter");
    request[QStringLiteral("units")] = QStringLiteral("kilometers");
    request[QStringLiteral("language")] = m_language;
    request[QStringLiteral("shape_format")] = QStringLiteral("polyline6");
    QJsonObject dirOpts;
    dirOpts[QStringLiteral("units")] = QStringLiteral("kilometers");
    dirOpts[QStringLiteral("language")] = m_language;
    request[QStringLiteral("directions_options")] = dirOpts;

    QNetworkRequest req(QUrl(m_endpoint + QStringLiteral("route")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(30000);

    // On embedded systems the clock may be wrong at boot, causing
    // "certificate not yet valid" errors. Disable peer verification
    // for the routing request (localhost or trusted public endpoint).
    QSslConfiguration ssl = req.sslConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(ssl);

    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    qDebug() << "ValhallaClient: POST" << req.url().toString() << body;
    auto *reply = m_nam.post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleRouteReply(reply);
    });
}

void ValhallaClient::checkStatus()
{
    QNetworkRequest req(QUrl(m_endpoint + QStringLiteral("status")));
    req.setTransferTimeout(3000);
    QSslConfiguration ssl = req.sslConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(ssl);

    auto *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        bool ok = (reply->error() == QNetworkReply::NoError);
        emit statusChecked(ok);
        reply->deleteLater();
    });
}

void ValhallaClient::handleRouteReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString msg;
        if (reply->error() == QNetworkReply::OperationCanceledError ||
            reply->error() == QNetworkReply::TimeoutError) {
            msg = QStringLiteral("Connection to routing server timed out");
        } else if (reply->error() == QNetworkReply::ConnectionRefusedError ||
                   reply->error() == QNetworkReply::HostNotFoundError) {
            msg = QStringLiteral("Cannot connect to routing server");
        } else if (status == 400) {
            msg = QStringLiteral("Invalid route request. Destination may be unreachable.");
        } else if (status == 429) {
            emit rateLimited();
            msg = QStringLiteral("Too many routing requests. Please wait a moment.");
        } else if (status >= 500) {
            msg = QStringLiteral("Routing server error. Please try again later.");
        } else {
            msg = reply->errorString();
        }
        emit routeError(msg);
        return;
    }

    QByteArray data = reply->readAll();
    Route route = RouteHelpers::parseRouteResponse(data);
    if (route.isValid()) {
        emit routeCalculated(route);
    } else {
        emit routeError(QStringLiteral("Failed to parse route response"));
    }
}

#include "ValhallaClient.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

ValhallaClient::ValhallaClient(QObject *parent)
    : QObject(parent)
    , m_endpoint(QStringLiteral("http://localhost:8002/"))
{
}

void ValhallaClient::setEndpoint(const QString &url)
{
    m_endpoint = url;
    if (!m_endpoint.endsWith(QLatin1Char('/')))
        m_endpoint.append(QLatin1Char('/'));
}

void ValhallaClient::calculateRoute(const LatLng &from, const LatLng &to)
{
    QJsonObject request;
    QJsonArray locations;
    locations.append(QJsonObject{{QStringLiteral("lat"), from.latitude},
                                  {QStringLiteral("lon"), from.longitude}});
    locations.append(QJsonObject{{QStringLiteral("lat"), to.latitude},
                                  {QStringLiteral("lon"), to.longitude}});
    request[QStringLiteral("locations")] = locations;
    request[QStringLiteral("costing")] = QStringLiteral("motor_scooter");
    request[QStringLiteral("units")] = QStringLiteral("kilometers");
    request[QStringLiteral("language")] = QStringLiteral("en-US");
    request[QStringLiteral("shape_format")] = QStringLiteral("polyline6");
    QJsonObject dirOpts;
    dirOpts[QStringLiteral("units")] = QStringLiteral("kilometers");
    dirOpts[QStringLiteral("language")] = QStringLiteral("en-US");
    request[QStringLiteral("directions_options")] = dirOpts;

    QNetworkRequest req(QUrl(m_endpoint + QStringLiteral("route")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(5000);

    auto *reply = m_nam.post(req, QJsonDocument(request).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleRouteReply(reply);
    });
}

void ValhallaClient::checkStatus()
{
    QNetworkRequest req(QUrl(m_endpoint + QStringLiteral("status")));
    req.setTransferTimeout(3000);

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
    Route route = parseRouteResponse(data);
    if (route.isValid()) {
        emit routeCalculated(route);
    } else {
        emit routeError(QStringLiteral("Failed to parse route response"));
    }
}

Route ValhallaClient::parseRouteResponse(const QByteArray &data)
{
    Route route;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "Valhalla JSON parse error:" << err.errorString();
        return route;
    }

    QJsonObject root = doc.object();
    QJsonObject trip = root[QStringLiteral("trip")].toObject();
    QJsonArray legs = trip[QStringLiteral("legs")].toArray();
    if (legs.isEmpty()) return route;

    QJsonObject leg = legs[0].toObject();
    QJsonObject summary = leg[QStringLiteral("summary")].toObject();

    // Decode shape (polyline, precision 6)
    QString shape = leg[QStringLiteral("shape")].toString();
    route.waypoints = decodePolyline(shape, 6);

    // Total distance/duration
    route.distance = summary[QStringLiteral("length")].toDouble() * 1000.0; // km → m
    route.duration = summary[QStringLiteral("time")].toDouble();

    // Parse maneuvers
    QJsonArray maneuvers = leg[QStringLiteral("maneuvers")].toArray();
    for (const auto &m : maneuvers) {
        QJsonObject obj = m.toObject();
        RouteInstruction instr;

        int typeCode = obj[QStringLiteral("type")].toInt();
        instr.type = mapValhallaType(typeCode);
        instr.distance = obj[QStringLiteral("length")].toDouble() * 1000.0; // km → m
        instr.duration = obj[QStringLiteral("time")].toDouble();
        instr.originalShapeIndex = obj[QStringLiteral("begin_shape_index")].toInt();
        instr.instructionText = obj[QStringLiteral("instruction")].toString();

        // Street names
        QJsonArray streets = obj[QStringLiteral("street_names")].toArray();
        if (!streets.isEmpty()) {
            instr.streetName = streets[0].toString();
        } else {
            QJsonArray beginStreets = obj[QStringLiteral("begin_street_names")].toArray();
            if (!beginStreets.isEmpty())
                instr.streetName = beginStreets[0].toString();
        }

        // Verbal instructions
        instr.verbalAlertInstruction =
            obj[QStringLiteral("verbal_transition_alert_instruction")].toString();
        instr.verbalPreTransitionInstruction =
            obj[QStringLiteral("verbal_pre_transition_instruction")].toString();
        instr.verbalSuccinctInstruction =
            obj[QStringLiteral("verbal_succinct_transition_instruction")].toString();
        instr.verbalMultiCue = obj[QStringLiteral("verbal_multi_cue")].toBool();

        // Roundabout
        if (typeCode == 26) {
            instr.roundaboutExitCount =
                obj[QStringLiteral("roundabout_exit_count")].toInt();
        }
        instr.bearingBefore = obj[QStringLiteral("begin_heading")].toDouble();
        instr.bearingAfter = obj[QStringLiteral("end_heading")].toDouble();

        // Set location from route waypoints
        if (instr.originalShapeIndex >= 0 && instr.originalShapeIndex < route.waypoints.size()) {
            instr.location = route.waypoints[instr.originalShapeIndex];
        }

        route.instructions.append(instr);
    }

    return route;
}

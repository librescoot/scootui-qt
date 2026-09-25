#include "RoutePlanService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <initializer_list>

namespace {
bool jsonCoordinate(const QJsonValue &value, double &out)
{
    if (value.isDouble()) {
        out = value.toDouble();
        return std::isfinite(out);
    }
    if (value.isString()) {
        bool ok = false;
        out = value.toString().toDouble(&ok);
        return ok;
    }
    return false;
}

// First present key wins. Channels differ on names: the canonical wire shape
// uses lat/lon, while cloud and app payloads often send latitude/longitude and
// label or name.
QJsonValue firstValue(const QJsonObject &object,
                      std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull())
            return value;
    }
    return {};
}

} // namespace

void RoutePlanService::assignIds(QList<RouteStop> &stops)
{
    for (int i = 0; i < stops.size(); ++i)
        stops[i].id = i + 1;
}

QString RoutePlanService::serializeWaypoints(const QList<RouteStop> &stops)
{
    QJsonArray array;
    for (const RouteStop &stop : stops) {
        if (!stop.position.isValid())
            continue;
        QJsonObject object;
        object[QStringLiteral("lat")] = stop.position.latitude;
        object[QStringLiteral("lon")] = stop.position.longitude;
        if (!stop.label.isEmpty())
            object[QStringLiteral("label")] = stop.label;
        array.append(object);
    }
    if (array.isEmpty())
        return {};
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QList<RouteStop> RoutePlanService::parseWaypoints(const QString &json, bool *ok)
{
    if (ok)
        *ok = false;

    QList<RouteStop> stops;
    const QString trimmed = json.trimmed();
    if (trimmed.isEmpty())
        return stops;

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return stops;

    for (const QJsonValue &value : doc.array()) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();

        double lat = 0;
        double lon = 0;
        if (!jsonCoordinate(firstValue(object, {"lat", "latitude"}), lat)
            || !jsonCoordinate(firstValue(object, {"lon", "longitude"}), lon)) {
            continue;
        }

        RouteStop stop;
        stop.position = {lat, lon};
        if (!stop.position.isValid())
            continue;
        stop.label = firstValue(object, {"label", "name"}).toString();
        stops.append(stop);
    }

    if (stops.isEmpty())
        return stops;

    assignIds(stops);
    if (ok)
        *ok = true;
    return stops;
}

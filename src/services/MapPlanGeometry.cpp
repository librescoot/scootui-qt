#include "MapPlanGeometry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include <algorithm>

namespace {

QJsonObject emptyFeatureCollection()
{
    QJsonObject collection;
    collection[QStringLiteral("type")] = QStringLiteral("FeatureCollection");
    collection[QStringLiteral("features")] = QJsonArray();
    return collection;
}

QJsonObject point(double latitude, double longitude)
{
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("Point")},
        {QStringLiteral("coordinates"), QJsonArray{longitude, latitude}}};
}

} // namespace

namespace MapPlanGeometry {

QString lineGeoJson(const QList<LatLng> &points)
{
    if (points.size() < 2)
        return QString::fromUtf8(QJsonDocument(emptyFeatureCollection()).toJson(QJsonDocument::Compact));

    QJsonArray coordinates;
    for (const LatLng &point : points)
        coordinates.append(QJsonArray{point.longitude, point.latitude});

    const QJsonObject feature{
        {QStringLiteral("type"), QStringLiteral("Feature")},
        {QStringLiteral("geometry"),
         QJsonObject{{QStringLiteral("type"), QStringLiteral("LineString")},
                     {QStringLiteral("coordinates"), coordinates}}}};
    return QString::fromUtf8(QJsonDocument(feature).toJson(QJsonDocument::Compact));
}

QString stopsGeoJson(const QVariantList &stops, int currentStep)
{
    QJsonArray features;
    for (int i = 0; i < stops.size(); ++i) {
        const QVariantMap stop = stops.at(i).toMap();
        const double latitude = stop.value(QStringLiteral("latitude")).toDouble();
        const double longitude = stop.value(QStringLiteral("longitude")).toDouble();
        if (!LatLng{latitude, longitude}.isValid())
            continue;

        QJsonObject properties;
        properties[QStringLiteral("index")] = i;
        // Numeric flags: layer filters compare numbers.
        properties[QStringLiteral("current")] = (i == currentStep) ? 1 : 0;
        properties[QStringLiteral("reached")] =
            stop.value(QStringLiteral("reached")).toBool() ? 1 : 0;
        properties[QStringLiteral("first")] = (i == 0) ? 1 : 0;
        properties[QStringLiteral("last")] = (i == stops.size() - 1) ? 1 : 0;
        properties[QStringLiteral("label")] = stop.value(QStringLiteral("label")).toString();

        features.append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("Feature")},
            {QStringLiteral("geometry"), point(latitude, longitude)},
            {QStringLiteral("properties"), properties}});
    }

    QJsonObject collection = emptyFeatureCollection();
    collection[QStringLiteral("features")] = features;
    return QString::fromUtf8(QJsonDocument(collection).toJson(QJsonDocument::Compact));
}

QString traveledGeoJson(const QList<LatLng> &route, int segment,
                        const LatLng &matchedPosition)
{
    if (route.size() < 2 || segment < 0)
        return lineGeoJson({});

    QList<LatLng> traveled = route.mid(0, std::min<qsizetype>(segment + 1, route.size()));
    if (segment < route.size() - 1 && matchedPosition.isValid()
        && (traveled.isEmpty() || traveled.last() != matchedPosition)) {
        traveled.append(matchedPosition);
    }
    return lineGeoJson(traveled);
}

QString overviewMarkersGeoJson(const LatLng &start, const LatLng &finish,
                               const LatLng &currentPosition)
{
    QJsonArray features;
    const auto addMarker = [&features](const LatLng &position, const QString &kind) {
        if (!position.isValid())
            return;
        features.append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("Feature")},
            {QStringLiteral("geometry"), point(position.latitude, position.longitude)},
            {QStringLiteral("properties"), QJsonObject{{QStringLiteral("kind"), kind}}}});
    };
    addMarker(start, QStringLiteral("start"));
    addMarker(finish, QStringLiteral("finish"));
    addMarker(currentPosition, QStringLiteral("current"));

    QJsonObject collection = emptyFeatureCollection();
    collection[QStringLiteral("features")] = features;
    return QString::fromUtf8(QJsonDocument(collection).toJson(QJsonDocument::Compact));
}

} // namespace MapPlanGeometry

#include "MapPlanGeometry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

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
    // GeoJSON is [longitude, latitude].
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
        // Numeric rather than boolean: the layer filter compares a number, which
        // is unambiguous across MapLibre filter implementations.
        properties[QStringLiteral("current")] = (i == currentStep) ? 1 : 0;
        properties[QStringLiteral("reached")] =
            stop.value(QStringLiteral("reached")).toBool() ? 1 : 0;
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

} // namespace MapPlanGeometry

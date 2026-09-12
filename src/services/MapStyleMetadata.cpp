#include "MapStyleMetadata.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {

QString colorKey(const QJsonObject &metadata, const char *key, const QString &fallback)
{
    const QString value = metadata.value(QLatin1String(key)).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

int widthKey(const QJsonObject &metadata, const char *key, int fallback)
{
    const QJsonValue value = metadata.value(QLatin1String(key));
    return value.isDouble() && value.toDouble() > 0 ? static_cast<int>(value.toDouble())
                                                    : fallback;
}

}

MapRouteStyle parseMapRouteStyle(const QByteArray &styleJson)
{
    MapRouteStyle style;
    const QJsonObject metadata = QJsonDocument::fromJson(styleJson)
                                     .object()
                                     .value(QStringLiteral("metadata"))
                                     .toObject();
    if (metadata.isEmpty())
        return style;

    style.fillColor = colorKey(metadata, "route-fill-color", style.fillColor);
    style.borderColor = colorKey(metadata, "route-border-color", style.borderColor);
    style.fillWidth = widthKey(metadata, "route-fill-width", style.fillWidth);
    style.borderWidth = widthKey(metadata, "route-border-width", style.borderWidth);
    return style;
}

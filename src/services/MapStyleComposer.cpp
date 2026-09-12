#include "MapStyleComposer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace {

void removeTraffic(QJsonObject &root)
{
    QJsonObject sources = root.value(QStringLiteral("sources")).toObject();
    sources.remove(QStringLiteral("google-traffic"));
    root[QStringLiteral("sources")] = sources;

    const QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    QJsonArray filtered;
    for (const QJsonValue &value : layers) {
        const QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("id")).toString() != QLatin1String("traffic-overlay"))
            filtered.append(layer);
    }
    root[QStringLiteral("layers")] = filtered;
}

void configureOfflineResources(QJsonObject &root, const MapStyleConfig &config)
{
    QJsonObject sources = root.value(QStringLiteral("sources")).toObject();
    const QString mbtilesUrl = QStringLiteral("mbtiles://") + config.mbtilesPath;
    for (auto it = sources.begin(); it != sources.end(); ++it) {
        QJsonObject source = it.value().toObject();
        if (source.value(QStringLiteral("type")).toString() != QLatin1String("vector"))
            continue;
        source.remove(QStringLiteral("tiles"));
        source[QStringLiteral("url")] = mbtilesUrl;
        source[QStringLiteral("maxzoom")] = 14;
        it.value() = source;
    }
    root[QStringLiteral("sources")] = sources;
    root.remove(QStringLiteral("sprite"));

    if (!config.glyphDirectory.isEmpty()) {
        root[QStringLiteral("glyphs")] = QUrl::fromLocalFile(config.glyphDirectory).toString()
            + QStringLiteral("/{fontstack}/{range}.pbf");
        return;
    }

    root.remove(QStringLiteral("glyphs"));
    const QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    QJsonArray filtered;
    for (const QJsonValue &value : layers) {
        const QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("type")).toString() != QLatin1String("symbol"))
            filtered.append(layer);
    }
    root[QStringLiteral("layers")] = filtered;
}

QJsonObject routeLine(const QString &id, const QString &color,
                      int width, double opacity)
{
    QJsonObject layout;
    layout[QStringLiteral("line-cap")] = QStringLiteral("round");
    layout[QStringLiteral("line-join")] = QStringLiteral("round");

    QJsonObject paint;
    paint[QStringLiteral("line-color")] = color;
    paint[QStringLiteral("line-width")] = width;
    if (opacity < 1.0)
        paint[QStringLiteral("line-opacity")] = opacity;

    QJsonObject layer;
    layer[QStringLiteral("id")] = id;
    layer[QStringLiteral("type")] = QStringLiteral("line");
    layer[QStringLiteral("source")] = QStringLiteral("route");
    layer[QStringLiteral("layout")] = layout;
    layer[QStringLiteral("paint")] = paint;
    return layer;
}

void injectRoute(QJsonObject &root, const MapRouteStyle &style)
{
    QJsonObject sources = root.value(QStringLiteral("sources")).toObject();
    QJsonObject featureCollection;
    featureCollection[QStringLiteral("type")] = QStringLiteral("FeatureCollection");
    featureCollection[QStringLiteral("features")] = QJsonArray();
    QJsonObject routeSource;
    routeSource[QStringLiteral("type")] = QStringLiteral("geojson");
    routeSource[QStringLiteral("data")] = featureCollection;
    sources[QStringLiteral("route")] = routeSource;
    root[QStringLiteral("sources")] = sources;

    QJsonArray layers;
    for (const QJsonValue &value : root.value(QStringLiteral("layers")).toArray()) {
        const QString id = value.toObject().value(QStringLiteral("id")).toString();
        if (id != QLatin1String("route-border")
            && id != QLatin1String("route-fill")
            && id != QLatin1String("route-ghost")) {
            layers.append(value);
        }
    }

    int firstExtrusion = -1;
    int lastExtrusion = -1;
    int firstSymbol = -1;
    for (int i = 0; i < layers.size(); ++i) {
        const QString type = layers.at(i).toObject().value(QStringLiteral("type")).toString();
        if (type == QLatin1String("fill-extrusion")) {
            if (firstExtrusion < 0)
                firstExtrusion = i;
            lastExtrusion = i;
        } else if (type == QLatin1String("symbol") && firstSymbol < 0) {
            firstSymbol = i;
        }
    }

    QJsonArray composed;
    for (int i = 0; i < layers.size(); ++i) {
        if (i == firstExtrusion || (firstExtrusion < 0 && i == firstSymbol)) {
            composed.append(routeLine(QStringLiteral("route-border"), style.borderColor,
                                      style.borderWidth, 1.0));
            composed.append(routeLine(QStringLiteral("route-fill"), style.fillColor,
                                      style.fillWidth, 1.0));
        }
        composed.append(layers.at(i));
        if (firstExtrusion >= 0 && i == lastExtrusion) {
            composed.append(routeLine(QStringLiteral("route-ghost"), style.fillColor,
                                      style.fillWidth, 0.35));
        }
    }
    if (firstExtrusion < 0 && firstSymbol < 0) {
        composed.append(routeLine(QStringLiteral("route-border"), style.borderColor,
                                  style.borderWidth, 1.0));
        composed.append(routeLine(QStringLiteral("route-fill"), style.fillColor,
                                  style.fillWidth, 1.0));
    }
    root[QStringLiteral("layers")] = composed;
}

} // namespace

QJsonObject flattenMapExtrusionLayer(QJsonObject layer)
{
    if (layer.value(QStringLiteral("type")).toString() != QLatin1String("fill-extrusion"))
        return layer;

    const QJsonObject sourcePaint = layer.value(QStringLiteral("paint")).toObject();
    QJsonObject paint;
    auto transfer = [&sourcePaint, &paint](const QString &from, const QString &to) {
        if (sourcePaint.contains(from))
            paint[to] = sourcePaint.value(from);
    };
    transfer(QStringLiteral("fill-extrusion-color"), QStringLiteral("fill-color"));
    transfer(QStringLiteral("fill-extrusion-opacity"), QStringLiteral("fill-opacity"));
    transfer(QStringLiteral("fill-extrusion-pattern"), QStringLiteral("fill-pattern"));
    transfer(QStringLiteral("fill-extrusion-translate"), QStringLiteral("fill-translate"));
    transfer(QStringLiteral("fill-extrusion-translate-anchor"),
             QStringLiteral("fill-translate-anchor"));

    layer[QStringLiteral("type")] = QStringLiteral("fill");
    layer[QStringLiteral("paint")] = paint;
    return layer;
}

MapStyleComposeResult composeMapStyle(const QByteArray &sourceJson,
                                      const MapStyleConfig &config)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(sourceJson, &parseError);
    if (!document.isObject()) {
        return {{}, QStringLiteral("invalid style JSON at offset %1: %2")
                         .arg(parseError.offset)
                         .arg(parseError.errorString())};
    }

    QJsonObject root = document.object();
    if (!config.revision.isEmpty()) {
        QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
        metadata[QStringLiteral("scootui-style-revision")] = config.revision;
        root[QStringLiteral("metadata")] = metadata;
    }

    if (!config.mbtilesPath.isEmpty())
        configureOfflineResources(root, config);
    if (!config.trafficVisible)
        removeTraffic(root);
    if (config.view2D) {
        QJsonArray layers;
        for (const QJsonValue &value : root.value(QStringLiteral("layers")).toArray())
            layers.append(flattenMapExtrusionLayer(value.toObject()));
        root[QStringLiteral("layers")] = layers;
    }

    injectRoute(root, config.route);
    return {QJsonDocument(root).toJson(QJsonDocument::Compact), {}};
}

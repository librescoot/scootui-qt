#pragma once

#include "services/MapStyleMetadata.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

struct MapStyleConfig
{
    QString mbtilesPath;
    QString glyphDirectory;
    QString revision;
    bool trafficVisible = true;
    bool view2D = false;
    MapRouteStyle route;
};

struct MapStyleComposeResult
{
    QByteArray json;
    QString error;

    explicit operator bool() const { return error.isEmpty(); }
};

MapStyleComposeResult composeMapStyle(const QByteArray &sourceJson,
                                      const MapStyleConfig &config);
QJsonObject flattenMapExtrusionLayer(QJsonObject layer);

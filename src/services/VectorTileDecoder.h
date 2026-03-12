#pragma once

#include <QByteArray>
#include <QPointF>
#include <QString>
#include <QVector>

namespace VectorTile {

struct Feature {
    int type = 0; // 1=POINT, 2=LINESTRING, 3=POLYGON
    QVector<uint32_t> geometry;
};

struct Layer {
    QString name;
    uint32_t extent = 4096;
    QVector<Feature> features;
};

struct Tile {
    QVector<Layer> layers;
};

// Decompress gzip-compressed tile data
QByteArray gunzip(const QByteArray &compressed);

// Parse a protobuf-encoded vector tile
Tile parse(const QByteArray &data);

// Decode a POINT geometry command stream to tile-local coordinates
QPointF decodePoint(const QVector<uint32_t> &geometry);

} // namespace VectorTile

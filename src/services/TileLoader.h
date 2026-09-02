#pragma once

#include <QObject>
#include <QString>
#include "VectorTileDecoder.h"

// Loads and decodes vector tiles on its own thread with its own sqlite
// connection (QSqlDatabase connections are thread-affine). Results are handed
// back by value and tagged with the generation of the file they came from.
class TileLoader : public QObject
{
    Q_OBJECT

public:
    explicit TileLoader(QObject *parent = nullptr);
    ~TileLoader();

public slots:
    void setPath(const QString &path, int generation);
    void load(quint64 key, int zoom, int generation);

signals:
    void loaded(quint64 key, const VectorTile::Tile &tile, int generation);
    void missing(quint64 key, int generation);

private:
    void closeDb();

    QString m_connectionName;
    QString m_path;
    int m_generation = 0;
    bool m_open = false;
};


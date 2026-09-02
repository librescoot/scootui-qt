#include "TileLoader.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

TileLoader::TileLoader(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("tile_loader_%1")
                           .arg(reinterpret_cast<quintptr>(this), 0, 16))
{
    qRegisterMetaType<VectorTile::Tile>("VectorTile::Tile");
}

TileLoader::~TileLoader()
{
    closeDb();
}

void TileLoader::closeDb()
{
    if (!m_open)
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_open = false;
}

void TileLoader::setPath(const QString &path, int generation)
{
    closeDb();
    m_path = path;
    m_generation = generation;
    if (path.isEmpty())
        return;
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    m_open = db.open();
    if (!m_open)
        qWarning() << "TileLoader: failed to open" << path;
}

void TileLoader::load(quint64 key, int zoom, int generation)
{
    if (generation != m_generation || !m_open) {
        emit missing(key, generation);
        return;
    }
    const int tileX = static_cast<int>(key >> 32);
    const int tileY = static_cast<int>(static_cast<uint32_t>(key & 0xffffffffu));

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?"));
    query.addBindValue(zoom);
    query.addBindValue(tileX);
    query.addBindValue(tileY);
    if (!query.exec() || !query.next()) {
        emit missing(key, generation);
        return;
    }
    const QByteArray decompressed = VectorTile::gunzip(query.value(0).toByteArray());
    if (decompressed.isEmpty()) {
        emit missing(key, generation);
        return;
    }
    emit loaded(key, VectorTile::parse(decompressed), generation);
}

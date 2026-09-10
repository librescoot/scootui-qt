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
    if (!QSqlDatabase::contains(m_connectionName))
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
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread()->isInterruptionRequested())
        return;
    closeDb();
    m_path = path;
    m_generation = generation;
    if (path.isEmpty())
        return;
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=100"));
    m_open = db.open();
    if (!m_open)
        qWarning() << "TileLoader: failed to open" << path;
}

void TileLoader::load(quint64 key, int zoom, int generation)
{
    const auto tile = read(key, zoom, generation);
    if (QThread::currentThread()->isInterruptionRequested())
        return;
    if (tile)
        emit loaded(key, *tile, generation);
    else
        emit missing(key, generation);
}

std::optional<VectorTile::Tile> TileLoader::read(quint64 key, int zoom, int generation,
                                               int maxCompressedBytes, int maxDecodedBytes)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread()->isInterruptionRequested()
        || generation != m_generation || !m_open)
        return std::nullopt;
    const int tileX = static_cast<int>(key >> 32);
    const int tileY = static_cast<int>(static_cast<uint32_t>(key & 0xffffffffu));
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    // CASE avoids copying an oversized blob into Qt. LIMIT also bounds malformed
    // databases with duplicate rows; it cannot bound a storage syscall's latency.
    query.prepare(QStringLiteral(
        "SELECT CASE WHEN ?=0 OR length(tile_data)<=? THEN tile_data END FROM tiles "
        "WHERE zoom_level=? AND tile_column=? AND tile_row=? LIMIT 1"));
    query.addBindValue(maxCompressedBytes);
    query.addBindValue(maxCompressedBytes);
    query.addBindValue(zoom);
    query.addBindValue(tileX);
    query.addBindValue(tileY);
    if (!query.exec() || !query.next() || QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;
    const QByteArray decompressed = VectorTile::gunzip(query.value(0).toByteArray(), maxDecodedBytes);
    if (decompressed.isEmpty() || QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;
    return VectorTile::parse(decompressed);
}

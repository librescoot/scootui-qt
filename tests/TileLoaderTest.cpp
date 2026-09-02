#include <QtTest>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <zlib.h>

#include "services/TileLoader.h"

namespace {

void varint(QByteArray &out, quint64 v)
{
    while (v >= 0x80) {
        out.append(static_cast<char>((v & 0x7f) | 0x80));
        v >>= 7;
    }
    out.append(static_cast<char>(v));
}

void lengthDelimited(QByteArray &out, int field, const QByteArray &payload)
{
    varint(out, static_cast<quint64>((field << 3) | 2));
    varint(out, static_cast<quint64>(payload.size()));
    out.append(payload);
}

void varintField(QByteArray &out, int field, quint64 v)
{
    varint(out, static_cast<quint64>(field << 3));
    varint(out, v);
}

quint32 zigzag(qint32 v)
{
    return static_cast<quint32>((v << 1) ^ (v >> 31));
}

// One layer "streets" with one LINESTRING feature: MoveTo(10,20) LineTo(+30,+40),
// tagged name=Teststrasse.
QByteArray buildTile()
{
    QByteArray geometry;
    for (quint32 v : {quint32(9), zigzag(10), zigzag(20), quint32(10), zigzag(30), zigzag(40)})
        varint(geometry, v);
    QByteArray tags;
    varint(tags, 0);
    varint(tags, 0);

    QByteArray feature;
    lengthDelimited(feature, 2, tags);
    varintField(feature, 3, 2);
    lengthDelimited(feature, 4, geometry);

    QByteArray value;
    lengthDelimited(value, 1, QByteArrayLiteral("Teststrasse"));

    QByteArray layer;
    varintField(layer, 15, 2);
    lengthDelimited(layer, 1, QByteArrayLiteral("streets"));
    lengthDelimited(layer, 2, feature);
    lengthDelimited(layer, 3, QByteArrayLiteral("name"));
    lengthDelimited(layer, 4, value);
    varintField(layer, 5, 4096);

    QByteArray tile;
    lengthDelimited(tile, 3, layer);
    return tile;
}

QByteArray gzipCompress(const QByteArray &in)
{
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
        return {};
    QByteArray out(static_cast<int>(deflateBound(&stream, in.size())), Qt::Uninitialized);
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.data()));
    stream.avail_in = static_cast<uInt>(in.size());
    stream.next_out = reinterpret_cast<Bytef *>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());
    const int ret = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (ret != Z_STREAM_END)
        return {};
    out.resize(static_cast<int>(stream.total_out));
    return out;
}

constexpr int Zoom = 14;
constexpr int TileX = 8802;
constexpr int TileY = 10985;

quint64 key(int x, int y)
{
    return (static_cast<quint64>(x) << 32) | static_cast<quint64>(static_cast<uint32_t>(y));
}

} // namespace

class TileLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<VectorTile::Tile>("VectorTile::Tile");
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("map.mbtiles"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          QStringLiteral("fixture"));
            db.setDatabaseName(m_path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE tiles (zoom_level INTEGER, tile_column INTEGER, "
                "tile_row INTEGER, tile_data BLOB)")));
            q.prepare(QStringLiteral(
                "INSERT INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?, ?, ?, ?)"));
            q.addBindValue(Zoom);
            q.addBindValue(TileX);
            q.addBindValue(TileY);
            q.addBindValue(gzipCompress(buildTile()));
            QVERIFY(q.exec());
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("fixture"));

        m_loader = new TileLoader;
        m_loader->moveToThread(&m_thread);
        connect(&m_thread, &QThread::finished, m_loader, &QObject::deleteLater);
        m_thread.start();
    }

    void cleanupTestCase()
    {
        m_thread.quit();
        m_thread.wait();
    }

    void loadsAndDecodesOnItsThread()
    {
        QSignalSpy loaded(m_loader, &TileLoader::loaded);
        QSignalSpy missing(m_loader, &TileLoader::missing);
        QMetaObject::invokeMethod(m_loader, "setPath", Qt::QueuedConnection,
                                  Q_ARG(QString, m_path), Q_ARG(int, 1));
        QMetaObject::invokeMethod(m_loader, "load", Qt::QueuedConnection,
                                  Q_ARG(quint64, key(TileX, TileY)), Q_ARG(int, Zoom), Q_ARG(int, 1));
        QVERIFY(loaded.wait(5000));
        QCOMPARE(missing.count(), 0);
        const auto args = loaded.takeFirst();
        QCOMPARE(args.at(0).toULongLong(), key(TileX, TileY));
        QCOMPARE(args.at(2).toInt(), 1);
        const auto tile = args.at(1).value<VectorTile::Tile>();
        QCOMPARE(tile.layers.size(), 1);
        QCOMPARE(tile.layers[0].name, QStringLiteral("streets"));
        QCOMPARE(tile.layers[0].features.size(), 1);
        const auto &feature = tile.layers[0].features[0];
        QCOMPARE(feature.type, 2);
        QCOMPARE(feature.properties.value(QStringLiteral("name")), QStringLiteral("Teststrasse"));
        const auto parts = VectorTile::decodeLineStringParts(feature.geometry);
        QCOMPARE(parts.size(), 1);
        QCOMPARE(parts[0].size(), 2);
        QCOMPARE(parts[0][0], QPointF(10, 20));
        QCOMPARE(parts[0][1], QPointF(40, 60));
    }

    void reportsMissingTiles()
    {
        QSignalSpy missing(m_loader, &TileLoader::missing);
        QMetaObject::invokeMethod(m_loader, "load", Qt::QueuedConnection,
                                  Q_ARG(quint64, key(TileX + 1, TileY)), Q_ARG(int, Zoom), Q_ARG(int, 1));
        QVERIFY(missing.wait(5000));
        QCOMPARE(missing.takeFirst().at(0).toULongLong(), key(TileX + 1, TileY));
    }

    void staleGenerationIsMissing()
    {
        QSignalSpy loaded(m_loader, &TileLoader::loaded);
        QSignalSpy missing(m_loader, &TileLoader::missing);
        QMetaObject::invokeMethod(m_loader, "load", Qt::QueuedConnection,
                                  Q_ARG(quint64, key(TileX, TileY)), Q_ARG(int, Zoom), Q_ARG(int, 7));
        QVERIFY(missing.wait(5000));
        QCOMPARE(missing.takeFirst().at(1).toInt(), 7);
        QCOMPARE(loaded.count(), 0);
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    QThread m_thread;
    TileLoader *m_loader = nullptr;
};

QTEST_GUILESS_MAIN(TileLoaderTest)
#include "TileLoaderTest.moc"

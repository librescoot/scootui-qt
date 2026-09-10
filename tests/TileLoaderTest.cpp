#include <QtTest>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <zlib.h>

#include "services/TileLoader.h"
#include "services/StreetQueryDispatcher.h"
#include <cmath>
#include <limits>

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
    varint(tags, 1);
    varint(tags, 1);

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
    lengthDelimited(layer, 3, QByteArrayLiteral("kind"));
    QByteArray kindValue;
    lengthDelimited(kindValue, 1, QByteArrayLiteral("residential"));
    lengthDelimited(layer, 4, kindValue);
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

    void boundedStreetExtractionAndMissingReload() {
        auto lon = [](double x) { return x / (1 << Zoom) * 360.0 - 180.0; };
        auto lat = [](double y) {
            return std::atan(std::sinh(M_PI * (2 * y / (1 << Zoom) - 1))) * 180 / M_PI;
        };
        StreetQueryRequest r;
        r.path = m_path; r.mapGeneration = 1;
        r.minLon = lon(TileX + 0.001); r.maxLon = lon(TileX + 0.02);
        r.minLat = lat(TileY + 0.98); r.maxLat = lat(TileY + 0.999);
        auto result = queryStreets(r);
        QVERIFY(result.complete);
        QCOMPARE(result.streets.size(), 1);
        const auto feature = result.streets[0].toMap();
        QCOMPARE(feature["name"].toString(), QString("Teststrasse"));
        QCOMPARE(feature["kind"].toString(), QString("residential"));
        QCOMPARE(feature["roundabout"].toBool(), false);
        const auto points = feature["points"].toList();
        QCOMPARE(points.size(), 2);
        QVERIFY(qAbs(points[0].toList()[0].toDouble() - lat(TileY + 1 - 20.0 / 4096)) < 1e-10);
        QVERIFY(qAbs(points[0].toList()[1].toDouble() - lon(TileX + 10.0 / 4096)) < 1e-10);
        r.mapGeneration = 2;
        r.path = m_dir.filePath("missing.mbtiles");
        QVERIFY(!queryStreets(r).complete);
        r.path = m_path;
        QCOMPARE(queryStreets(r).streets.size(), 1);
        r.minLon = lon(TileX + 1.001); r.maxLon = lon(TileX + 1.02);
        result = queryStreets(r);
        QVERIFY(!result.complete);
        QVERIFY(result.streets.isEmpty());
        r.minLon = -180; r.maxLon = 179;
        QVERIFY(queryStreets(r).streets.isEmpty());
        r.minLon = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(queryStreets(r).streets.isEmpty());
    }

    void iconDecodeLimitsAndMalformedInput() {
        const auto raw = buildTile();
        const auto compressed = gzipCompress(raw);
        QVERIFY(VectorTile::gunzip(compressed, raw.size() - 1).isEmpty());
        QCOMPARE(VectorTile::gunzip(compressed, raw.size()), raw);
        TileLoader loader;
        loader.setPath(m_path, 1);
        QVERIFY(!loader.read(key(TileX, TileY), Zoom, 1, compressed.size() - 1, 1024));
        QVERIFY(!loader.read(key(TileX, TileY), Zoom, 1, 1024, raw.size() - 1));
        QVERIFY(loader.read(key(TileX, TileY), Zoom, 1, 1024, 1024).has_value());
        // Oversized varints and length fields must terminate without invalid shifts
        // or pointer arithmetic. Sanitizer runs exercise these through the parser.
        QVERIFY(VectorTile::parse(QByteArray(20, char(0xff))).layers.isEmpty());
        QVERIFY(VectorTile::parse(QByteArray::fromHex("1affffffffffffffffff7f")).layers.size() <= 1);
        VectorTile::parse(QByteArray::fromHex("1a080dffffffffffffffff"));
    }

    void connectionRemovedOnAutonomousShutdown_data()
    {
        QTest::addColumn<bool>("validPath");
        QTest::newRow("open database") << true;
        QTest::newRow("failed open") << false;
    }

    void connectionRemovedOnAutonomousShutdown()
    {
        QFETCH(bool, validPath);
        auto *thread = new QThread;
        auto *loader = new TileLoader;
        const QString connection = QStringLiteral("tile_loader_%1")
            .arg(reinterpret_cast<quintptr>(loader), 0, 16);
        loader->moveToThread(thread);
        connect(thread, &QThread::finished, loader, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        QSignalSpy deleted(loader, &QObject::destroyed);
        QSignalSpy threadDeleted(thread, &QObject::destroyed);
        QSignalSpy missing(loader, &TileLoader::missing);
        thread->start();
        const QString path = validPath ? m_path : m_dir.filePath("absent/map.mbtiles");
        QMetaObject::invokeMethod(loader, "setPath", Qt::QueuedConnection,
                                  Q_ARG(QString, path), Q_ARG(int, 1));
        QMetaObject::invokeMethod(loader, "load", Qt::QueuedConnection,
                                  Q_ARG(quint64, key(TileX + 1, TileY)),
                                  Q_ARG(int, Zoom), Q_ARG(int, 1));
        QVERIFY(missing.wait(5000));
        QVERIFY(QSqlDatabase::contains(connection));
        thread->requestInterruption();
        thread->quit();
        QTRY_COMPARE(deleted.count(), 1);
        QTRY_COMPARE(threadDeleted.count(), 1);
        QVERIFY(!QSqlDatabase::contains(connection));
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

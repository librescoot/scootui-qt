#include <QtTest>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include "utils/MapDeltaPatcher.h"

namespace {

QString sha(const QByteArray &d)
{
    return QString::fromLatin1(QCryptographicHash::hash(d, QCryptographicHash::Sha256).toHex());
}

QByteArray slurp(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void put(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(data), data.size());
}

void beBytes(QByteArray &b, quint64 v, int n)
{
    for (int i = n - 1; i >= 0; --i) b.append(char((v >> (8 * i)) & 0xff));
}

// An MBTiles file with every value kind the canonical stream distinguishes:
// NULL, '' and an empty blob must stay three different things.
void makeMap(const QString &path, const QByteArray &tileFill, bool extraRow = false)
{
    const QString name = QStringLiteral("mapdelta_test_") + QUuid::createUuid().toString(QUuid::Id128);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE metadata (name text, value text, UNIQUE (name))"));
        QVERIFY(q.exec("CREATE TABLE tiles (zoom_level integer, tile_column integer, tile_row integer, tile_data blob)"));
        QVERIFY(q.exec("CREATE UNIQUE INDEX tile_index ON tiles (zoom_level, tile_column, tile_row)"));
        QVERIFY(q.exec("CREATE TABLE odd (id INTEGER PRIMARY KEY, a TEXT, b BLOB, c REAL, d INTEGER, e TEXT)"));
        QVERIFY(q.exec("INSERT INTO metadata VALUES ('name','test'), ('format','pbf')"));
        QVERIFY(q.exec("INSERT INTO odd VALUES (1, NULL, NULL, NULL, NULL, 'ü€')"));
        QVERIFY(q.exec("INSERT INTO odd VALUES (2, '', x'', 0.5, -9223372036854775807, '')"));
        QVERIFY(q.exec("INSERT INTO odd VALUES (3, 'text', x'00ff10', 3.141592653589793, 42, NULL)"));
        for (int i = 0; i < 40; ++i) {
            QSqlQuery t(db);
            QVERIFY(t.prepare("INSERT INTO tiles VALUES (?, ?, ?, ?)"));
            t.addBindValue(10 + i % 3);
            t.addBindValue(i);
            t.addBindValue(i * 2);
            QByteArray payload = tileFill + QByteArray::number(i);
            if (extraRow && i == 7) payload += "changed";
            t.addBindValue(MapDeltaPatcher::gzip(payload));
            QVERIFY(t.exec());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}

QString canonicalOf(const QString &map, const QString &out)
{
    QString error;
    const QString digest = MapDeltaPatcher::canonicalize(map, out, {}, &error);
    if (digest.isEmpty()) qWarning() << error;
    return digest;
}

void writeContainer(const QString &path, const QJsonObject &header, const QByteArray &payload)
{
    const QByteArray json = QJsonDocument(header).toJson(QJsonDocument::Compact);
    QByteArray out("MDP1");
    beBytes(out, quint64(json.size()), 4);
    out += json;
    beBytes(out, quint64(payload.size()), 8);
    out += payload;
    put(path, out);
}

QJsonObject headerFor(const QString &baseCanon, const QString &targetCanon, const QString &baseSha = QString(64, 'a'))
{
    return {
        {"algorithm", "hdiffpatch"},
        {"format", "mbtiles-canonical-1"},
        {"region", "bremen"},
        {"base_sha256", baseSha},
        {"base_canonical_sha256", sha(slurp(baseCanon))},
        {"target_canonical_sha256", sha(slurp(targetCanon))},
        {"base_canonical_size", double(QFileInfo(baseCanon).size())},
        {"target_canonical_size", double(QFileInfo(targetCanon).size())},
        {"target_size", 12345.0},
    };
}

}  // namespace

class MapDeltaPatcherTest : public QObject
{
    Q_OBJECT
private slots:
    void rebuildReproducesEveryValueKind();
    void matchesTheReferenceImplementation();
    void appliesADelta();
    void rejectsBadDeltas();
    void gzipRoundTrip();
    void realPatchWhenAvailable();
    void realReleaseWhenAvailable();
};

void MapDeltaPatcherTest::gzipRoundTrip()
{
    const QByteArray data = QByteArray(5000, 'x') + "tail";
    bool ok = false;
    QCOMPARE(MapDeltaPatcher::gunzip(MapDeltaPatcher::gzip(data), &ok), data);
    QVERIFY(ok);
    MapDeltaPatcher::gunzip("not gzip", &ok);
    QVERIFY(!ok);
}

void MapDeltaPatcherTest::rebuildReproducesEveryValueKind()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    makeMap(d.filePath("a.mbtiles"), "tile");
    const QString first = canonicalOf(d.filePath("a.mbtiles"), d.filePath("a.canon"));
    QVERIFY(!first.isEmpty());
    const auto built = MapDeltaPatcher::rebuild(d.filePath("a.canon"), d.filePath("b.mbtiles"), {});
    QVERIFY2(built.ok(), qPrintable(built.error));
    // Not byte identical (tiles are recompressed) but canonically identical.
    QCOMPARE(canonicalOf(d.filePath("b.mbtiles"), d.filePath("b.canon")), first);
    QCOMPARE(slurp(d.filePath("b.canon")), slurp(d.filePath("a.canon")));
}

void MapDeltaPatcherTest::matchesTheReferenceImplementation()
{
    const QString tool = qEnvironmentVariable("CANONICAL_TOOL");
    if (tool.isEmpty()) QSKIP("Set CANONICAL_TOOL to tools/canonical-mbtiles.py to cross-check the writer");
    QTemporaryDir d; QVERIFY(d.isValid());
    makeMap(d.filePath("a.mbtiles"), "tile");
    const QString mine = canonicalOf(d.filePath("a.mbtiles"), d.filePath("a.canon"));
    QProcess python;
    python.start(QStringLiteral("python3"), {tool, d.filePath("a.mbtiles"), d.filePath("ref.canon"), QStringLiteral("--print-hash")});
    QVERIFY(python.waitForFinished(60000));
    QCOMPARE(python.exitCode(), 0);
    QCOMPARE(QString::fromUtf8(python.readAllStandardOutput()).trimmed(), mine);
    QCOMPARE(slurp(d.filePath("ref.canon")), slurp(d.filePath("a.canon")));
}

void MapDeltaPatcherTest::appliesADelta()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    makeMap(d.filePath("base.mbtiles"), "tile");
    makeMap(d.filePath("target.mbtiles"), "tile", true);
    QVERIFY(!canonicalOf(d.filePath("base.mbtiles"), d.filePath("base.canon")).isEmpty());
    const QString targetDigest = canonicalOf(d.filePath("target.mbtiles"), d.filePath("target.canon"));
    // A stand-in decoder that "applies" the patch by emitting the known target.
    put(d.filePath("hpatchz"), "#!/bin/sh\ncp '" + d.filePath("target.canon").toUtf8() + "' \"$4\"\n");
    QVERIFY(QFile::setPermissions(d.filePath("hpatchz"), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    writeContainer(d.filePath("x.hdelta"), headerFor(d.filePath("base.canon"), d.filePath("target.canon")), "ignored");

    const auto result = MapDeltaPatcher::applyDelta(d.filePath("hpatchz"), d.filePath("base.mbtiles"),
                                                    d.filePath("x.hdelta"), d.filePath("out.mbtiles"),
                                                    d.path(), QString(64, 'a'), {});
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(canonicalOf(d.filePath("out.mbtiles"), d.filePath("out.canon")), targetDigest);
    // Scratch streams are gone; only the inputs and the result remain.
    QVERIFY(QDir(d.path()).entryList({"map-*"}, QDir::Files).isEmpty());
}

void MapDeltaPatcherTest::rejectsBadDeltas()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    makeMap(d.filePath("base.mbtiles"), "tile");
    makeMap(d.filePath("target.mbtiles"), "tile", true);
    canonicalOf(d.filePath("base.mbtiles"), d.filePath("base.canon"));
    canonicalOf(d.filePath("target.mbtiles"), d.filePath("target.canon"));
    put(d.filePath("hpatchz"), "#!/bin/sh\ncp '" + d.filePath("target.canon").toUtf8() + "' \"$4\"\n");
    put(d.filePath("badpatchz"), "#!/bin/sh\nprintf wrong > \"$4\"\n");
    put(d.filePath("failpatchz"), "#!/bin/sh\nexit 3\n");
    for (const char *n : {"hpatchz", "badpatchz", "failpatchz"})
        QVERIFY(QFile::setPermissions(d.filePath(n), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    const auto run = [&](const QString &decoder, QJsonObject header, const QString &expectedBase = QString(64, 'a')) {
        writeContainer(d.filePath("x.hdelta"), header, "p");
        const auto r = MapDeltaPatcher::applyDelta(d.filePath(decoder), d.filePath("base.mbtiles"),
                                                   d.filePath("x.hdelta"), d.filePath("out.mbtiles"),
                                                   d.path(), expectedBase, {});
        QVERIFY(!r.ok());
        QVERIFY(!QFile::exists(d.filePath("out.mbtiles")));
        QVERIFY(QDir(d.path()).entryList({"map-*"}, QDir::Files).isEmpty());
    };
    const auto good = headerFor(d.filePath("base.canon"), d.filePath("target.canon"));
    auto h = good; h["base_canonical_sha256"] = QString(64, '0');
    run("hpatchz", h);                                         // installed map is not the base
    run("hpatchz", good, QString(64, 'b'));                    // manifest names another base
    h = good; h["target_canonical_sha256"] = QString(64, '1');
    run("hpatchz", h);                                         // reconstruction does not verify
    run("badpatchz", good);                                    // decoder wrote the wrong size
    run("failpatchz", good);                                   // decoder failed
    h = good; h["algorithm"] = "xdelta3";
    run("hpatchz", h);                                         // unknown algorithm
    h = good; h["format"] = "mbtiles-canonical-2";
    run("hpatchz", h);                                         // unknown format
    h = good; h.remove("target_canonical_size");
    run("hpatchz", h);                                         // incomplete header

    writeContainer(d.filePath("x.hdelta"), good, "p");
    const auto cancelled = MapDeltaPatcher::applyDelta(d.filePath("hpatchz"), d.filePath("base.mbtiles"),
                                                       d.filePath("x.hdelta"), d.filePath("out.mbtiles"),
                                                       d.path(), QString(64, 'a'), [] { return true; });
    QCOMPARE(cancelled.result, MapDeltaPatcher::Result::Cancelled);
    QVERIFY(QDir(d.path()).entryList({"map-*"}, QDir::Files).isEmpty());
}

void MapDeltaPatcherTest::realPatchWhenAvailable()
{
    const QString encoder = qEnvironmentVariable("HDIFFZ"), decoder = qEnvironmentVariable("HPATCHZ");
    if (encoder.isEmpty() || decoder.isEmpty()) QSKIP("Set HDIFFZ and HPATCHZ for real decoder integration");
    QTemporaryDir d; QVERIFY(d.isValid());
    makeMap(d.filePath("base.mbtiles"), "tile");
    makeMap(d.filePath("target.mbtiles"), "tile", true);
    canonicalOf(d.filePath("base.mbtiles"), d.filePath("base.canon"));
    const QString targetDigest = canonicalOf(d.filePath("target.mbtiles"), d.filePath("target.canon"));
    QCOMPARE(QProcess::execute(encoder, {"-c-zstd-20", d.filePath("base.canon"), d.filePath("target.canon"), d.filePath("p.hdiff")}), 0);
    writeContainer(d.filePath("x.hdelta"), headerFor(d.filePath("base.canon"), d.filePath("target.canon")), slurp(d.filePath("p.hdiff")));
    const auto r = MapDeltaPatcher::applyDelta(decoder, d.filePath("base.mbtiles"), d.filePath("x.hdelta"),
                                               d.filePath("out.mbtiles"), d.path(), QString(64, 'a'), {});
    QVERIFY2(r.ok(), qPrintable(r.error));
    QCOMPARE(canonicalOf(d.filePath("out.mbtiles"), d.filePath("out.canon")), targetDigest);
}

void MapDeltaPatcherTest::realReleaseWhenAvailable()
{
    const QString base = qEnvironmentVariable("MAP_BASE"), delta = qEnvironmentVariable("MAP_DELTA");
    const QString decoder = qEnvironmentVariable("HPATCHZ");
    if (base.isEmpty() || delta.isEmpty() || decoder.isEmpty()) QSKIP("Release fixtures not configured");
    QTemporaryDir d; QVERIFY(d.isValid());
    MapDeltaPatcher::Header h;
    QString error;
    QVERIFY2(MapDeltaPatcher::readHeader(delta, &h, &error), qPrintable(error));
    QElapsedTimer timer; timer.start();
    const auto r = MapDeltaPatcher::applyDelta(decoder, base, delta, d.filePath("out.mbtiles"), d.path(), h.baseSha256, {});
    qInfo() << "applyDelta" << timer.elapsed() << "ms, result" << int(r.result) << r.error
            << "output bytes" << QFileInfo(d.filePath("out.mbtiles")).size() << "expected tiles file bytes" << h.targetSize;
    QVERIFY2(r.ok(), qPrintable(r.error));
    // Independent check: canonicalising the result must reproduce the target hash.
    QCOMPARE(canonicalOf(d.filePath("out.mbtiles"), d.filePath("out.canon")), h.targetCanonicalSha256);
}

QTEST_GUILESS_MAIN(MapDeltaPatcherTest)
#include "MapDeltaPatcherTest.moc"

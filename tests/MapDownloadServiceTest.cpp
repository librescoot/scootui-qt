#include <QtTest>
#include <QBuffer>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include "services/MapDownloadService.h"

class FakeReply : public QNetworkReply
{
public:
    FakeReply(const QNetworkRequest &request, QByteArray bytes, QObject *parent)
        : QNetworkReply(parent), m_bytes(std::move(bytes)) {
        setRequest(request); setUrl(request.url()); open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        QTimer::singleShot(0, this, [this] { emit readyRead(); setFinished(true); emit finished(); });
    }
    void abort() override { setError(OperationCanceledError, QStringLiteral("cancelled")); }
    qint64 bytesAvailable() const override { return m_bytes.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 maxSize) override {
        const qint64 n = std::min(maxSize, m_bytes.size() - m_offset);
        if (n <= 0) return -1;
        memcpy(data, m_bytes.constData() + m_offset, size_t(n)); m_offset += n; return n;
    }
private:
    QByteArray m_bytes;
    qint64 m_offset = 0;
};

class FakeNetwork : public QNetworkAccessManager
{
public:
    QHash<QString, QByteArray> responses;
    QStringList requests;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        const QString url=request.url().toString(); requests.append(url);
        return new FakeReply(request, responses.value(url), this);
    }
};

namespace {
void beBytes(QByteArray &b, quint64 v, int n)
{
    for (int i = n - 1; i >= 0; --i) b.append(char((v >> (8 * i)) & 0xff));
}

QByteArray slurp(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void makeMap(const QString &path, bool changed)
{
    const QString name = QStringLiteral("mds_test_") + QUuid::createUuid().toString(QUuid::Id128);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE metadata (name text, value text, UNIQUE (name))"));
        QVERIFY(q.exec("CREATE TABLE tiles (zoom_level integer, tile_column integer, tile_row integer, tile_data blob)"));
        QVERIFY(q.exec("INSERT INTO metadata VALUES ('name','test')"));
        for (int i = 0; i < 20; ++i) {
            QSqlQuery t(db);
            QVERIFY(t.prepare("INSERT INTO tiles VALUES (?, ?, ?, ?)"));
            t.addBindValue(12); t.addBindValue(i); t.addBindValue(i);
            t.addBindValue(MapDeltaPatcher::gzip("tile" + QByteArray::number(i) + (changed && i == 3 ? "new" : "")));
            QVERIFY(t.exec());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}
}  // namespace

class MapDownloadServiceTest : public QObject
{
    Q_OBJECT
private:
    static QString sha(const QByteArray &v) {
        return QString::fromLatin1(QCryptographicHash::hash(v, QCryptographicHash::Sha256).toHex());
    }
    static void put(const QString &path, const QByteArray &bytes) {
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(bytes), bytes.size());
    }
    void runScenario(bool wrongBase, bool corruptOutput, bool corruptDelta = false, bool staleBase = false, bool biggerDelta = false, bool noDecoder = false);
    void runMapScenario(bool corruptDelta, bool staleBase, bool noDecoder, bool badResult);
private slots:
    void smallerDeltaInstalls() { runScenario(false, false); }
    void wrongBaseUsesFull() { runScenario(true, false); }
    void badResultFallsBack() { runScenario(false, true); }
    void badDownloadFallsBack() { runScenario(false, false, true); }
    void changedBaseFallsBack() { runScenario(false, false, false, true); }
    void largerDeltaUsesFull() { runScenario(false, false, false, false, true); }
    void missingDecoderUsesFull() { runScenario(false, false, false, false, false, true); }
    void mapDeltaInstalls() { runMapScenario(false, false, false, false); }
    void mapDeltaCorruptDownloadUsesFull() { runMapScenario(true, false, false, false); }
    void mapDeltaChangedBaseUsesFull() { runMapScenario(false, true, false, false); }
    void mapDeltaMissingDecoderUsesFull() { runMapScenario(false, false, true, false); }
    void mapDeltaBadResultUsesFull() { runMapScenario(false, false, false, true); }
};

void MapDownloadServiceTest::runScenario(bool wrongBase, bool corruptOutput, bool corruptDelta, bool staleBase, bool biggerDelta, bool noDecoder)
{
    QTemporaryDir d; QVERIFY(d.isValid());
    const QByteArray oldData(256,'a'), newData(256,'b'), delta("small-patch");
    const QString decoder=d.filePath("hpatchz"), target=d.filePath("target"), systemctl=d.filePath("systemctl");
    put(target, corruptOutput ? QByteArray(256,'x') : newData);
    put(decoder, "#!/bin/sh\ncp '" + target.toUtf8() + "' \"$4\"\n");
    put(systemctl, "#!/bin/sh\nexit 0\n");
    QVERIFY(QFile::setPermissions(decoder,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner));
    QVERIFY(QFile::setPermissions(systemctl,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner));
    if (noDecoder) QVERIFY(QFile::remove(decoder));
    const QByteArray oldPath=qgetenv("PATH"); qputenv("PATH", noDecoder ? d.path().toUtf8() : d.path().toUtf8()+":"+oldPath);
    put(d.filePath("tiles.tar"),staleBase ? QByteArray(256,'c') : oldData);
    MapMetadata meta; meta.region=QStringLiteral("bremen"); meta.valhallaTiles=MapTileInfo{};
    meta.valhallaTiles->digest=sha(oldData); meta.valhallaTiles->version=QStringLiteral("tiles-20261001T000000Z");
    QVERIFY(MapMetadata::save(meta,d.filePath("metadata.json")));
    MapDownloadService service(nullptr,nullptr,d.path());
    auto *net = new FakeNetwork; net->setParent(&service); service.m_nam=net;
    const QString manifestUrl=QStringLiteral("https://downloads.librescoot.org/releases/maps-routing.json");
    const QString fullUrl=QStringLiteral("https://example.invalid/full.tar"), deltaUrl=QStringLiteral("https://example.invalid/delta.hdelta");
    QJsonObject diff{{"algorithm","hdiffpatch"},{"base_version","tiles-20261001T000000Z"},
                     {"base_sha256",sha(wrongBase ? QByteArray("other") : oldData)},
                     {"sha256",sha(delta)},{"url",deltaUrl},{"size",double(biggerDelta ? newData.size() : delta.size())}};
    QJsonObject routing{{"version","tiles-20261101T000000Z"},{"sha256",sha(newData)},
                        {"url",fullUrl},{"size",double(newData.size())},{"deltas",QJsonArray{diff}}};
    QJsonObject manifest{{"version",1},{"regions",QJsonObject{{"bremen",QJsonObject{{"routing",routing}}}}}};
    net->responses[manifestUrl]=QJsonDocument(manifest).toJson(); net->responses[deltaUrl]=corruptDelta ? QByteArray("bad-patch") : delta; net->responses[fullUrl]=newData;
    service.m_needsRouting=true; service.m_displayDone=true; service.m_routingDone=false;
    QSignalSpy completed(&service,&MapDownloadService::downloadComplete);
    service.doFetchReleases(false,true);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,10000);
    QFile result(d.filePath("tiles.tar")); QVERIFY(result.open(QIODevice::ReadOnly)); QCOMPARE(result.readAll(),newData);
    QCOMPARE(net->requests.contains(deltaUrl),!wrongBase && !biggerDelta && !noDecoder);
    QCOMPARE(net->requests.contains(fullUrl),wrongBase || corruptOutput || corruptDelta || staleBase || biggerDelta || noDecoder);
    QCOMPARE(service.m_metadata.valhallaTiles->version,QStringLiteral("tiles-20261101T000000Z"));
    QCOMPARE(service.m_metadata.valhallaTiles->digest,sha(newData));
    qputenv("PATH",oldPath);
}

void MapDownloadServiceTest::runMapScenario(bool corruptDelta, bool staleBase, bool noDecoder, bool badResult)
{
    QTemporaryDir d; QVERIFY(d.isValid());
    const QString baseMap = d.filePath("base-src.mbtiles"), targetMap = d.filePath("target.mbtiles");
    makeMap(baseMap, false);
    makeMap(targetMap, true);
    const QByteArray baseBytes = slurp(baseMap), targetBytes = slurp(targetMap);
    QString err;
    const QString baseCanonHash = MapDeltaPatcher::canonicalize(baseMap, d.filePath("base.canon"), {}, &err);
    const QString targetCanonHash = MapDeltaPatcher::canonicalize(targetMap, d.filePath("target.canon"), {}, &err);
    QVERIFY(!baseCanonHash.isEmpty() && !targetCanonHash.isEmpty());

    QJsonObject header{{"algorithm", "hdiffpatch"}, {"format", "mbtiles-canonical-1"}, {"region", "bremen"},
                       {"base_sha256", sha(baseBytes)}, {"base_canonical_sha256", baseCanonHash},
                       {"target_canonical_sha256", targetCanonHash},
                       {"base_canonical_size", double(QFileInfo(d.filePath("base.canon")).size())},
                       {"target_canonical_size", double(QFileInfo(d.filePath("target.canon")).size())},
                       {"target_size", double(targetBytes.size())}};
    const QByteArray json = QJsonDocument(header).toJson(QJsonDocument::Compact);
    QByteArray container("MDP1");
    beBytes(container, quint64(json.size()), 4); container += json;
    beBytes(container, 5, 8); container += "patch";

    // The decoder stand-in emits the known target stream (or garbage).
    const QString decoder = d.filePath("hpatchz"), systemctl = d.filePath("systemctl");
    put(decoder, badResult ? QByteArray("#!/bin/sh\nprintf wrong > \"$4\"\n")
                           : "#!/bin/sh\ncp '" + d.filePath("target.canon").toUtf8() + "' \"$4\"\n");
    put(systemctl, "#!/bin/sh\nexit 0\n");
    QVERIFY(QFile::setPermissions(decoder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    QVERIFY(QFile::setPermissions(systemctl, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    if (noDecoder) QVERIFY(QFile::remove(decoder));
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", noDecoder ? d.path().toUtf8() : d.path().toUtf8() + ":" + oldPath);

    QDir().mkpath(d.filePath("store"));
    const QString store = d.filePath("store");
    QFile::copy(baseMap, store + "/map.mbtiles");
    if (staleBase) { QFile::remove(store + "/map.mbtiles"); makeMap(store + "/map.mbtiles", true); }
    MapMetadata meta; meta.region = QStringLiteral("bremen"); meta.displayTiles = MapTileInfo{};
    meta.displayTiles->digest = sha(baseBytes);
    meta.displayTiles->version = QStringLiteral("tiles-20261001T000000Z");
    QVERIFY(MapMetadata::save(meta, store + "/metadata.json"));

    MapDownloadService service(nullptr, nullptr, store);
    auto *net = new FakeNetwork; net->setParent(&service); service.m_nam = net;
    const QString manifestUrl = QStringLiteral("https://downloads.librescoot.org/releases/maps-routing.json");
    const QString fullUrl = QStringLiteral("https://example.invalid/map.mbtiles"), deltaUrl = QStringLiteral("https://example.invalid/map.hdelta");
    const QByteArray served = corruptDelta ? QByteArray("corrupted") : container;
    QJsonObject delta{{"algorithm", "hdiffpatch"}, {"base_version", "tiles-20261001T000000Z"},
                      {"base_sha256", sha(baseBytes)}, {"sha256", sha(container)},
                      {"url", deltaUrl}, {"size", double(container.size())}};
    QJsonObject map{{"version", "tiles-20261101T000000Z"}, {"sha256", sha(targetBytes)},
                    {"url", fullUrl}, {"size", double(targetBytes.size() * 2)}, {"deltas", QJsonArray{delta}}};
    QJsonObject manifest{{"version", 1}, {"regions", QJsonObject{{"bremen", QJsonObject{{"map", map}}}}}};
    net->responses[manifestUrl] = QJsonDocument(manifest).toJson();
    net->responses[deltaUrl] = served;
    net->responses[fullUrl] = targetBytes;

    service.m_resolvedSlug = QStringLiteral("bremen");
    service.m_needsDisplay = true; service.m_displayDone = false;
    service.m_needsRouting = false; service.m_routingDone = true;
    QSignalSpy completed(&service, &MapDownloadService::downloadComplete);
    service.doFetchReleases(true, false);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 20000);

    const bool deltaWorked = !corruptDelta && !staleBase && !noDecoder && !badResult;
    // A map swapped behind the metadata's back still looks like the base, so
    // its delta is fetched; the canonical hash in the patch is what rejects it.
    QCOMPARE(net->requests.contains(deltaUrl), !noDecoder);
    QCOMPARE(net->requests.contains(fullUrl), !deltaWorked);
    const QString installed = store + "/map.mbtiles";
    if (deltaWorked) {
        QCOMPARE(MapDeltaPatcher::canonicalize(installed, d.filePath("out.canon"), {}, &err), targetCanonHash);
    } else {
        QCOMPARE(slurp(installed), targetBytes);
    }
    // Either way the recorded identity is the published one, so the next check
    // does not report the update again.
    QCOMPARE(service.m_metadata.displayTiles->digest, sha(targetBytes));
    QCOMPARE(service.m_metadata.displayTiles->version, QStringLiteral("tiles-20261101T000000Z"));
    QVERIFY(QDir(store + "/.download").entryList({"map-*", "*.hdelta.part", "display.mbtiles.part*"}, QDir::Files).isEmpty());
    qputenv("PATH", oldPath);
}

QTEST_GUILESS_MAIN(MapDownloadServiceTest)
#include "MapDownloadServiceTest.moc"

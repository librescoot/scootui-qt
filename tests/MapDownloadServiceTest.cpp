#include <QtTest>
#include <QBuffer>
#include <QEventLoop>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <zstd.h>
#include "services/MapDownloadService.h"
#include "services/MapDownloadHold.h"
#include "services/TileCapabilityPolicy.h"
#include "services/StrictJson.h"

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
    void runScenario(bool wrongBase, bool corruptOutput, bool corruptDelta = false, bool staleBase = false, bool biggerDelta = false, bool noDecoder = false, bool unknownRequirement = false, bool v2 = false);
    void runCompressedScenario(int mode);
    void runMapScenario(bool corruptDelta, bool staleBase, bool noDecoder, bool badResult, int mode = 0);
private slots:
    void smallerDeltaInstalls() { runScenario(false, false); }
    void wrongBaseUsesFull() { runScenario(true, false); }
    void badResultFallsBack() { runScenario(false, true); }
    void badDownloadFallsBack() { runScenario(false, false, true); }
    void changedBaseFallsBack() { runScenario(false, false, false, true); }
    void largerDeltaUsesFull() { runScenario(false, false, false, false, true); }
    void missingDecoderUsesFull() { runScenario(false, false, false, false, false, true); }
    void unknownDeltaRequirementUsesFull() { runScenario(false, false, false, false, false, false, true); }
    void v2DeltaInstalls() { runScenario(false, false, false, false, false, false, false, true); }
    void v2UnsupportedDeltaUsesFull() { runScenario(false, false, false, false, false, false, true, true); }
    void compressedRoutingInstalls() { runCompressedScenario(0); }
    void badCompressedDigestUsesFull() { runCompressedScenario(1); }
    void unsupportedCompressedCodecUsesFull() { runCompressedScenario(2); }
    void wrongCompressedRegionUsesFull() { runCompressedScenario(3); }
    void compressedOutputMismatchKeepsPrevious() { runCompressedScenario(4); }
    void missingTargetDigestUsesFull() { runCompressedScenario(5); }
    void fractionalCompressedSizeUsesFull() { runCompressedScenario(6); }
    void unknownCompressedRequirementUsesFull() { runCompressedScenario(7); }
    void declaredCompressedRequirementUsesFull() { runCompressedScenario(8); }
    void unknownRoutingRequirementFailsClosed() { runCompressedScenario(9); }
    void caseAliasRequirementUsesFull() { runCompressedScenario(10); }
    void v2CompressedRoutingInstalls() { runCompressedScenario(11); }
    void v2UnsupportedCompressedUsesFull() { runCompressedScenario(12); }
    void v2WrongCompressedTargetUsesFull() { runCompressedScenario(13); }
    void v2UnsupportedFullFailsClosed() { runCompressedScenario(14); }
    void v2CapabilityPolicyRejectsUnknownAndMismatchedAssets() {
        const QString mapHash(64, QLatin1Char('a')), baseHash(64, QLatin1Char('b'));
        const QString region = QStringLiteral("graz");
        const QSet<QString> caps{QStringLiteral("map.format.mbtiles-mvt-v1"),
                                 QStringLiteral("map-delta.format.mbtiles-canonical-v1"),
                                 QStringLiteral("patch.algorithm.hdiffpatch")};
        QJsonObject map{{"url", "https://example.invalid/map.mbtiles"}, {"size", 100}, {"sha256", mapHash},
                        {"kind", "map"}, {"region", "graz"}, {"format", "mbtiles-mvt-v1"},
                        {"requires", QJsonArray{"map.format.mbtiles-mvt-v1"}}};
        QVERIFY(TileCapabilityPolicy::supports(map, QStringLiteral("map"), region, caps));
        auto invalid = map;
        invalid.insert(QStringLiteral("requires"), QJsonArray{"map.format.mbtiles-mvt-v1", "future.codec"});
        QVERIFY(!TileCapabilityPolicy::supports(invalid, QStringLiteral("map"), region, caps));
        invalid.insert(QStringLiteral("requires"), QJsonArray{"map.format.mbtiles-mvt-v1", "map.format.mbtiles-mvt-v1"});
        QVERIFY(!TileCapabilityPolicy::supports(invalid, QStringLiteral("map"), region, caps));
        invalid = map; invalid.insert(QStringLiteral("format"), QStringLiteral("future"));
        QVERIFY(!TileCapabilityPolicy::supports(invalid, QStringLiteral("map"), region, caps));
        invalid = map; invalid.remove(QStringLiteral("requires"));
        QVERIFY(!TileCapabilityPolicy::supports(invalid, QStringLiteral("map"), region, caps));
        invalid = map; invalid.insert(QStringLiteral("url"), QStringLiteral("http://example.invalid/map"));
        QVERIFY(!TileCapabilityPolicy::supports(invalid, QStringLiteral("map"), region, caps));
        invalid = map; invalid.insert(QStringLiteral("region"), QStringLiteral("vienna"));
        QVERIFY(!TileCapabilityPolicy::supports(invalid, QStringLiteral("map"), region, caps));
        QJsonObject delta{{"url", "https://example.invalid/map.hdelta"}, {"size", 10}, {"sha256", QString(64, QLatin1Char('c'))},
                          {"kind", "map-delta"}, {"region", "graz"}, {"format", "mbtiles-canonical-v1"}, {"algorithm", "hdiffpatch"},
                          {"base_version", "tiles-20261101T000000Z"}, {"base_sha256", baseHash}, {"target_sha256", mapHash},
                          {"requires", QJsonArray{"map-delta.format.mbtiles-canonical-v1", "patch.algorithm.hdiffpatch"}}};
        QVERIFY(TileCapabilityPolicy::supports(delta, QStringLiteral("map-delta"), region, caps, mapHash, baseHash));
        QVERIFY(!TileCapabilityPolicy::supports(delta, QStringLiteral("map-delta"), region, caps, mapHash, QString(64, QLatin1Char('d'))));
        QVERIFY(!TileCapabilityPolicy::supports(delta, QStringLiteral("map-delta"), region, caps, QString(64, QLatin1Char('e')), baseHash));
        delta.insert(QStringLiteral("requires"), QJsonArray{"map-delta.format.mbtiles-canonical-v1"});
        QVERIFY(!TileCapabilityPolicy::supports(delta, QStringLiteral("map-delta"), region, caps, mapHash, baseHash));
        const QSet<QString> routingCaps{QStringLiteral("routing-compressed.format.zstd")};
        QJsonObject compressed{{"url", "https://example.invalid/routing.tar.zst"}, {"size", 10},
                               {"sha256", QString(64, QLatin1Char('c'))}, {"target_sha256", mapHash},
                               {"kind", "routing-compressed"}, {"region", "graz"},
                               {"codec", "zstd"}, {"format", "zstd"},
                               {"requires", QJsonArray{"routing-compressed.format.zstd"}}};
        QVERIFY(TileCapabilityPolicy::supports(compressed, QStringLiteral("routing-compressed"), region, routingCaps, mapHash));
        QVERIFY(!TileCapabilityPolicy::supports(compressed, QStringLiteral("routing-compressed"), region, routingCaps, baseHash));
        compressed.insert(QStringLiteral("codec"), QStringLiteral("future"));
        QVERIFY(!TileCapabilityPolicy::supports(compressed, QStringLiteral("routing-compressed"), region, routingCaps, mapHash));
    }
    void strictV2JsonRejectsDuplicateKeys() {
        QJsonObject parsed;
        QVERIFY(StrictJson::parseObject(R"({"version":2,"regions":{}})", &parsed));
        QVERIFY(!StrictJson::parseObject(R"({"version":2,"version":1})", &parsed));
        QVERIFY(!StrictJson::parseObject(R"({"requires":[],"requ\u0069res":["future"]})", &parsed));
        QVERIFY(!StrictJson::parseObject(R"({"x":1} {"x":2})", &parsed));
        QVERIFY(!StrictJson::parseObject(R"({"x":[1,]})", &parsed));
    }
    void qualifiedV2FeedEnforcesAssetRequirements() {
        QTemporaryDir d; QVERIFY(d.isValid());
        const auto marker = d.filePath("capabilities");
        put(marker, "v1\nmap.format.mbtiles-mvt-v1\nrouting.format.valhalla-3.6.3\nrouting-compressed.format.zstd\n");
        MapDownloadService service(nullptr, nullptr, d.path());
        service.m_tileCapabilitiesPath = marker;
        auto *net = new FakeNetwork; net->setParent(&service); service.m_nam = net;
        const QString url = QStringLiteral("https://downloads.librescoot.org/releases/v2/maps-routing.json");
        const QString hash(64, QLatin1Char('a'));
        QJsonObject map{{"url", "https://example.org/map.mbtiles"}, {"size", 12}, {"sha256", hash},
                        {"kind", "map"}, {"region", "graz"}, {"format", "mbtiles-mvt-v1"},
                        {"requires", QJsonArray{"map.format.mbtiles-mvt-v1"}}};
        QJsonObject routing{{"url", "https://example.org/tiles.tar"}, {"size", 100}, {"sha256", hash},
                            {"kind", "routing"}, {"region", "graz"}, {"format", "valhalla-3.6.3"},
                            {"requires", QJsonArray{"routing.format.valhalla-3.6.3"}}};
        QJsonObject packed{{"url", "https://example.org/tiles.tar.zst"}, {"size", 10}, {"sha256", QString(64, 'b')},
                           {"kind", "routing-compressed"}, {"region", "graz"}, {"format", "zstd"}, {"codec", "zstd"},
                           {"target_sha256", hash}, {"requires", QJsonArray{"routing-compressed.format.zstd"}}};
        routing.insert("compressed", packed);
        auto fetch = [&](const QJsonObject &m, const QJsonObject &r) {
            QJsonObject root{{"version", 2}, {"regions", QJsonObject{{"graz", QJsonObject{{"map", m}, {"routing", r}}}}}};
            net->responses[url] = QJsonDocument(root).toJson();
            QEventLoop loop; QJsonObject result;
            service.fetchTilesManifest([&](const QJsonObject &value) { result = value; loop.quit(); });
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);
            loop.exec();
            return result;
        };
        auto result = fetch(map, routing);
        auto region = result.value("graz").toObject();
        QCOMPARE(region.value("map").toObject().value("sha256").toString(), hash);
        QVERIFY(!region.value("map").toObject().contains("requires"));
        QVERIFY(!region.value("valhalla").toObject().value("compressed").toObject().isEmpty());
        map.insert("requires", QJsonArray{"map.format.mbtiles-mvt-v1", "future.codec"});
        packed.insert("target_sha256", QString(64, 'c'));
        routing.insert("compressed", packed);
        region = fetch(map, routing).value("graz").toObject();
        QVERIFY(region.value("map").isNull());
        QVERIFY(!region.value("valhalla").toObject().contains("compressed"));
        map.insert("requires", QJsonArray{"map.format.mbtiles-mvt-v1"});
        map.insert("Requires", QJsonArray{"future.codec"});
        QVERIFY(fetch(map, routing).value("graz").toObject().value("map").isNull());
        QVERIFY(net->requests.contains(url));
        QVERIFY(!net->requests.contains(QStringLiteral("https://downloads.librescoot.org/releases/maps-routing.json")));
        net->responses[url] = R"({"version":2,"regions":{},"regions":{"graz":{}}})";
        bool done = false; QJsonObject rejected;
        service.fetchTilesManifest([&](const QJsonObject &value) { rejected = value; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(rejected.isEmpty());
        net->responses[url] = R"({"version":2,"requires":["future.index"],"regions":{"graz":{}}})";
        done = false;
        service.fetchTilesManifest([&](const QJsonObject &value) { rejected = value; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(rejected.isEmpty());
    }
    void producerGeneratedV2Index() {
        const QString fixture = qEnvironmentVariable("LIBRESCOOT_MAP_V2_FIXTURE");
        if (fixture.isEmpty()) QSKIP("set LIBRESCOOT_MAP_V2_FIXTURE to a generated index");
        QFile file(fixture); QVERIFY(file.open(QIODevice::ReadOnly));
        QTemporaryDir d; QVERIFY(d.isValid());
        MapDownloadService service(nullptr, nullptr, d.path());
        service.m_tileCapabilitiesPath = d.filePath("capabilities");
        put(service.m_tileCapabilitiesPath, "v1\nmap.format.mbtiles-mvt-v1\nrouting.format.valhalla-3.6.3\nrouting-compressed.format.zstd\n");
        auto *net = new FakeNetwork; net->setParent(&service); service.m_nam = net;
        const QString url = QStringLiteral("https://downloads.librescoot.org/releases/v2/maps-routing.json");
        net->responses[url] = file.readAll();
        bool done = false; QJsonObject result;
        service.fetchTilesManifest([&](const QJsonObject &value) { result = value; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        const auto region = result.value("graz").toObject();
        QCOMPARE(region.value("map").toObject().value("sha256").toString(), QString(64, 'a'));
        const auto routing = region.value("valhalla").toObject();
        QCOMPARE(routing.value("sha256").toString(), QString(64, 'b'));
        QCOMPARE(routing.value("compressed").toObject().value("sha256").toString(), QString(64, 'e'));
    }
    void invalidCapabilityDeclarationDoesNotUseLegacy() {
        QTemporaryDir d; QVERIFY(d.isValid());
        MapDownloadService service(nullptr, nullptr, d.path());
        service.m_tileCapabilitiesPath = d.filePath("capabilities");
        put(service.m_tileCapabilitiesPath, "v1\nfuture.codec\n");
        auto *net = new FakeNetwork; net->setParent(&service); service.m_nam = net;
        bool done = false; QJsonObject result;
        service.fetchTilesManifest([&](const QJsonObject &value) { result = value; done = true; });
        QVERIFY(done); QVERIFY(result.isEmpty()); QVERIFY(net->requests.isEmpty());
    }
    void v2PreviewRejectsUnsupportedFullAsset() {
        QTemporaryDir d; QVERIFY(d.isValid());
        MapDownloadService service(nullptr, nullptr, d.path());
        service.m_tileCapabilitiesPath = d.filePath("capabilities");
        put(service.m_tileCapabilitiesPath, "v1\nrouting.format.valhalla-3.6.3\n");
        auto *net = new FakeNetwork; net->setParent(&service); service.m_nam = net;
        const QString url = QStringLiteral("https://downloads.librescoot.org/releases/v2/maps-routing.json");
        QJsonObject routing{{"url", "https://example.org/tiles.tar"}, {"size", 100}, {"sha256", QString(64, 'a')},
                            {"kind", "routing"}, {"region", "graz"}, {"format", "valhalla-3.6.3"},
                            {"requires", QJsonArray{"routing.format.valhalla-3.6.3", "future.graph"}}};
        QJsonObject root{{"version", 2}, {"regions", QJsonObject{{"graz", QJsonObject{{"routing", routing}}}}}};
        net->responses[url] = QJsonDocument(root).toJson();
        service.m_resolvedSlug = "graz";
        service.m_metadata.valhallaTiles = MapTileInfo{QString(64, 'b'), {}, 100};
        QSignalSpy completed(&service, &MapDownloadService::updateCheckCompleted);
        service.checkForUpdates();
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 5000);
        QCOMPARE(qvariant_cast<MapDownloadService::UpdateCheckResult>(completed.first().first()),
                 MapDownloadService::UpdateCheckResult::Failed);
        QVERIFY(!service.updateAvailable());
    }
    void unsupportedManifestDoesNotFallBack() {
        QTemporaryDir d; QVERIFY(d.isValid());
        MapDownloadService service(nullptr, nullptr, d.path());
        auto *net = new FakeNetwork; net->setParent(&service); service.m_nam = net;
        const QString url = QStringLiteral("https://downloads.librescoot.org/releases/maps-routing.json");
        const QString legacy = QStringLiteral("https://downloads.librescoot.org/releases/tiles.json");
        net->responses[url] = QByteArrayLiteral("{\"version\":2,\"regions\":{}} ");
        net->responses[legacy] = QByteArrayLiteral("{\"bremen\":{}}");
        bool done = false; QJsonObject result;
        service.fetchTilesManifest([&](const QJsonObject &manifest) { result = manifest; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(result.isEmpty());
        QVERIFY(!net->requests.contains(legacy));
    }
    void holdFollowsStatus() {
        using S = ScootEnums::MapDownloadStatus;
        QCOMPARE(mapDownloadHoldFor(int(S::Downloading)), QStringLiteral("map-download"));
        QCOMPARE(mapDownloadHoldFor(int(S::Installing)), QStringLiteral("map-install"));
        // Several heartbeats must fit inside vehicle-service's 10 minute install cap.
        static_assert(MapInstallHeartbeatMs * 5 <= 10 * 60 * 1000);
        for (S idle : {S::Idle, S::Locating, S::CheckingUpdates, S::Done, S::Error})
            QVERIFY(mapDownloadHoldFor(int(idle)).isEmpty());
    }
    void mapDeltaInstalls() { runMapScenario(false, false, false, false); }
    void mapDeltaResumesFromPartAfterRestart() { runMapScenario(false, false, false, false, 1); }
    void mapDeltaRestartsAfterInterruptedPatch() { runMapScenario(false, false, false, false, 2); }
    void mapDeltaCorruptDownloadUsesFull() { runMapScenario(true, false, false, false); }
    void mapDeltaChangedBaseUsesFull() { runMapScenario(false, true, false, false); }
    void mapDeltaMissingDecoderUsesFull() { runMapScenario(false, false, true, false); }
    void mapDeltaBadResultUsesFull() { runMapScenario(false, false, false, true); }
};

void MapDownloadServiceTest::runCompressedScenario(int mode)
{
    QTemporaryDir d; QVERIFY(d.isValid());
    const QByteArray oldData(256, 'a'), newData(512, 'b');
    const QString systemctl = d.filePath("systemctl");
    put(systemctl, "#!/bin/sh\nexit 0\n");
    QVERIFY(QFile::setPermissions(systemctl, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", d.path().toUtf8() + ':' + oldPath);
    QByteArray compressed(qsizetype(ZSTD_compressBound(size_t(newData.size()))), Qt::Uninitialized);
    const size_t compressedSize = ZSTD_compress(compressed.data(), size_t(compressed.size()), newData.constData(), size_t(newData.size()), 1);
    QVERIFY(!ZSTD_isError(compressedSize)); compressed.resize(qsizetype(compressedSize));
    put(d.filePath("tiles.tar"), oldData);
    MapMetadata meta; meta.region = QStringLiteral("bremen"); meta.valhallaTiles = MapTileInfo{};
    meta.valhallaTiles->digest = sha(oldData);
    QVERIFY(MapMetadata::save(meta, d.filePath("metadata.json")));
    MapDownloadService service(nullptr, nullptr, d.path());
    auto *net = new FakeNetwork; net->setParent(&service); service.m_nam = net;
    const bool v2 = mode >= 11;
    if (v2) {
        service.m_tileCapabilitiesPath = d.filePath("capabilities");
        put(service.m_tileCapabilitiesPath, "v1\nrouting.format.valhalla-3.6.3\nrouting-compressed.format.zstd\n");
    }
    const QString manifestUrl = v2 ? QStringLiteral("https://downloads.librescoot.org/releases/v2/maps-routing.json")
                                   : QStringLiteral("https://downloads.librescoot.org/releases/maps-routing.json");
    const QString fullUrl = QStringLiteral("https://example.invalid/tiles.tar");
    const QString compressedUrl = mode == 3 ? QStringLiteral("https://example.invalid/other.tar.zst")
                                            : QStringLiteral("https://example.invalid/tiles.tar.zst");
    QJsonObject packed{{"codec", mode == 2 ? "future" : "zstd"}, {"url", compressedUrl},
                       {"sha256", mode == 1 ? "bad" : sha(compressed)}, {"size", double(compressed.size())}};
    if (mode == 6) packed.insert(QStringLiteral("size"), 1.5);
    if (mode == 7) packed.insert(QStringLiteral("requires"), QJsonArray{"routing-compressed.format.zstd", "future.decoder"});
    if (mode == 8) packed.insert(QStringLiteral("requires"), QJsonArray{"routing-compressed.format.zstd"});
    if (mode == 10) packed.insert(QStringLiteral("Requires"), QJsonArray{"future.decoder"});
    QJsonObject routing{{"version", "tiles-20261101T000000Z"}, {"sha256", mode == 5 ? QString() : sha(mode == 4 ? QByteArray("wrong") : newData)},
                        {"url", fullUrl}, {"size", double(newData.size())}, {"compressed", packed}};
    if (mode == 9) routing.insert(QStringLiteral("requires"), QJsonArray{"routing.format.future"});
    if (v2) {
        routing.insert("kind", "routing"); routing.insert("region", "bremen");
        routing.insert("format", "valhalla-3.6.3");
        routing.insert("requires", mode == 14 ? QJsonArray{"routing.format.valhalla-3.6.3", "future.graph"}
                                              : QJsonArray{"routing.format.valhalla-3.6.3"});
        packed.insert("kind", "routing-compressed"); packed.insert("region", "bremen");
        packed.insert("format", "zstd"); packed.insert("target_sha256", mode == 13 ? sha(oldData) : sha(newData));
        packed.insert("requires", mode == 12 ? QJsonArray{"routing-compressed.format.zstd", "future.codec"}
                                             : QJsonArray{"routing-compressed.format.zstd"});
        routing.insert("compressed", packed);
    }
    QJsonObject manifest{{"version", v2 ? 2 : 1}, {"regions", QJsonObject{{"bremen", QJsonObject{{"routing", routing}}}}}};
    net->responses[manifestUrl] = QJsonDocument(manifest).toJson();
    net->responses[fullUrl] = newData; net->responses[compressedUrl] = compressed;
    service.m_resolvedSlug = QStringLiteral("bremen"); service.m_needsRouting = true;
    service.m_displayDone = true; service.m_routingDone = false;
    QSignalSpy completed(&service, &MapDownloadService::downloadComplete);
    service.doFetchReleases(false, true);
    if (mode == 4 || mode == 9 || mode == 14) {
        QTRY_COMPARE_WITH_TIMEOUT(int(service.status()), int(ScootEnums::MapDownloadStatus::Error), 10000);
        QCOMPARE(slurp(d.filePath("tiles.tar")), oldData);
        if (mode == 4) QCOMPARE(service.m_rejectedCompressedDigest, sha(compressed));
        QVERIFY(!net->requests.contains(fullUrl));
    } else {
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 10000);
        QCOMPARE(slurp(d.filePath("tiles.tar")), newData);
        const bool useCompressed = mode == 0 || mode == 11;
        QCOMPARE(net->requests.contains(compressedUrl), useCompressed);
        QCOMPARE(net->requests.contains(fullUrl), !useCompressed);
    }
    qputenv("PATH", oldPath);
}

void MapDownloadServiceTest::runScenario(bool wrongBase, bool corruptOutput, bool corruptDelta, bool staleBase, bool biggerDelta, bool noDecoder, bool unknownRequirement, bool v2)
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
    if (v2) {
        service.m_tileCapabilitiesPath = d.filePath("capabilities");
        put(service.m_tileCapabilitiesPath, "v1\nrouting.format.valhalla-3.6.3\nrouting-delta.format.valhalla-3.6.3\npatch.algorithm.hdiffpatch\n");
    }
    const QString manifestUrl = v2 ? QStringLiteral("https://downloads.librescoot.org/releases/v2/maps-routing.json")
                                   : QStringLiteral("https://downloads.librescoot.org/releases/maps-routing.json");
    const QString fullUrl=QStringLiteral("https://example.invalid/full.tar"), deltaUrl=QStringLiteral("https://example.invalid/delta.hdelta");
    QJsonObject diff{{"algorithm","hdiffpatch"},{"base_version","tiles-20261001T000000Z"},
                     {"base_sha256",sha(wrongBase ? QByteArray("other") : oldData)},
                     {"sha256",sha(delta)},{"url",deltaUrl},{"size",double(biggerDelta ? newData.size() : delta.size())}};
    if (unknownRequirement && !v2) diff.insert(QStringLiteral("requires"), QJsonArray{"future.patch"});
    if (v2) {
        diff.insert("kind", "routing-delta"); diff.insert("region", "bremen");
        diff.insert("format", "valhalla-3.6.3"); diff.insert("target_sha256", sha(newData));
        QJsonArray requirements{"routing-delta.format.valhalla-3.6.3", "patch.algorithm.hdiffpatch"};
        if (unknownRequirement) requirements.append("future.patch");
        diff.insert("requires", requirements);
    }
    QJsonObject routing{{"version","tiles-20261101T000000Z"},{"sha256",sha(newData)},
                        {"url",fullUrl},{"size",double(newData.size())},{"deltas",QJsonArray{diff}}};
    if (v2) {
        routing.insert("kind", "routing"); routing.insert("region", "bremen");
        routing.insert("format", "valhalla-3.6.3");
        routing.insert("requires", QJsonArray{"routing.format.valhalla-3.6.3"});
    }
    QJsonObject manifest{{"version",v2 ? 2 : 1},{"regions",QJsonObject{{"bremen",QJsonObject{{"routing",routing}}}}}};
    net->responses[manifestUrl]=QJsonDocument(manifest).toJson(); net->responses[deltaUrl]=corruptDelta ? QByteArray("bad-patch") : delta; net->responses[fullUrl]=newData;
    service.m_needsRouting=true; service.m_displayDone=true; service.m_routingDone=false;
    QSignalSpy completed(&service,&MapDownloadService::downloadComplete);
    service.doFetchReleases(false,true);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,10000);
    QFile result(d.filePath("tiles.tar")); QVERIFY(result.open(QIODevice::ReadOnly)); QCOMPARE(result.readAll(),newData);
    QCOMPARE(net->requests.contains(deltaUrl),!wrongBase && !biggerDelta && !noDecoder && !unknownRequirement);
    QCOMPARE(net->requests.contains(fullUrl),wrongBase || corruptOutput || corruptDelta || staleBase || biggerDelta || noDecoder || unknownRequirement);
    QCOMPARE(service.m_metadata.valhallaTiles->version,QStringLiteral("tiles-20261101T000000Z"));
    QCOMPARE(service.m_metadata.valhallaTiles->digest,sha(newData));
    qputenv("PATH",oldPath);
}

// mode 0: normal; 1: a complete delta part and stale scratch from a previous
// session are on disk; 2: the first attempt is cancelled mid-patch, then retried.
void MapDownloadServiceTest::runMapScenario(bool corruptDelta, bool staleBase, bool noDecoder, bool badResult, int mode)
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
    const QByteArray slow = mode == 2 ? "sleep 4\n" : "";
    put(decoder, badResult ? QByteArray("#!/bin/sh\nprintf wrong > \"$4\"\n")
                           : "#!/bin/sh\n" + slow + "cp '" + d.filePath("target.canon").toUtf8() + "' \"$4\"\n");
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

    if (mode == 1) {
        QDir().mkpath(store + "/.download");
        put(store + "/.download/display." + sha(container) + ".hdelta.part", container);
        put(store + "/.download/map-deadbeef0000.canon", "half a stream");
        put(store + "/.download/map-deadbeef0000.hdiff", "stale patch");
        put(store + "/.download/display.mbtiles.part.7", "half a rebuilt map");
    }
    MapDownloadService service(nullptr, nullptr, store);
    if (mode == 1) {
        QVERIFY(service.hasPartialDisplayDownload());
        QVERIFY(QDir(store + "/.download").entryList({"map-*", "display.mbtiles.part.*"}, QDir::Files).isEmpty());
    }
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
    const QByteArray installedBefore = slurp(store + "/map.mbtiles");
    service.doFetchReleases(true, false);
    if (mode == 2) {
        // Let the patch start, then pull the plug on it.
        QTRY_COMPARE_WITH_TIMEOUT(int(service.status()), int(ScootEnums::MapDownloadStatus::Installing), 10000);
        QTest::qWait(1200);
        service.cancel();
        QTest::qWait(600);
        QCOMPARE(completed.size(), 0);
        // The installed map is untouched and nothing is left half built.
        QCOMPARE(slurp(store + "/map.mbtiles"), installedBefore);
        QVERIFY(QDir(store + "/.download").entryList({"map-*", "display.mbtiles.part.*"}, QDir::Files).isEmpty());
        // The downloaded patch survives for the retry.
        QVERIFY(service.hasPartialDisplayDownload());
        net->requests.clear();
        // What startDownload() does for a fresh attempt after a cancel.
        service.m_cancelled = false;
        ++service.m_operationGeneration;
        service.m_needsDisplay = true; service.m_displayDone = false;
        service.m_needsRouting = false; service.m_routingDone = true;
        service.doFetchReleases(true, false);
    }
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 20000);
    if (mode != 0) {
        // Resumed from the part file: the patch is not fetched a second time.
        QVERIFY(!net->requests.contains(deltaUrl));
        QVERIFY(!net->requests.contains(fullUrl));
    }

    const bool deltaWorked = !corruptDelta && !staleBase && !noDecoder && !badResult;
    // A map swapped behind the metadata's back still looks like the base, so
    // its delta is fetched; the canonical hash in the patch is what rejects it.
    if (mode == 0) {
        QCOMPARE(net->requests.contains(deltaUrl), !noDecoder);
        QCOMPARE(net->requests.contains(fullUrl), !deltaWorked);
    }
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

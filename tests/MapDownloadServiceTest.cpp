#include <QtTest>
#include <QBuffer>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTimer>
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
private slots:
    void smallerDeltaInstalls() { runScenario(false, false); }
    void wrongBaseUsesFull() { runScenario(true, false); }
    void badResultFallsBack() { runScenario(false, true); }
    void badDownloadFallsBack() { runScenario(false, false, true); }
    void changedBaseFallsBack() { runScenario(false, false, false, true); }
    void largerDeltaUsesFull() { runScenario(false, false, false, false, true); }
    void missingDecoderUsesFull() { runScenario(false, false, false, false, false, true); }
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

QTEST_GUILESS_MAIN(MapDownloadServiceTest)
#include "MapDownloadServiceTest.moc"

#include <QtTest>
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <atomic>
#include <thread>
#include <chrono>
#include "utils/HDiffPatcher.h"

class HDiffPatcherTest : public QObject
{
    Q_OBJECT
private:
    static QString sha(const QByteArray &data) {
        return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    }
    static void put(const QString &path, const QByteArray &data) {
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(data), data.size());
    }
private slots:
    void verifiesBaseAndTarget();
    void cancelledOrFailedLeavesNoOutput();
    void cancellationStopsDecoder();
    void realPatchWhenAvailable();
    void realReleaseWhenAvailable();
};

void HDiffPatcherTest::verifiesBaseAndTarget()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    const auto base = d.filePath("base"), patch = d.filePath("patch"), out = d.filePath("out"), tool = d.filePath("hpatchz");
    put(base, "old"); put(patch, "new content"); put(tool, "#!/bin/sh\ncp \"$3\" \"$4\"\n");
    QVERIFY(QFile::setPermissions(tool, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    auto result = HDiffPatcher::apply(tool, base, patch, out, sha("old"), sha("new content"), 11, [] { return false; });
    QVERIFY2(result.ok(), qPrintable(result.error));
    result = HDiffPatcher::apply(tool, base, patch, out, sha("bad"), sha("new content"), 11, [] { return false; });
    QVERIFY(!result.ok()); QVERIFY(!QFile::exists(out));
    result = HDiffPatcher::apply(tool, base, patch, out, sha("old"), sha("bad"), 11, [] { return false; });
    QVERIFY(!result.ok()); QVERIFY(!QFile::exists(out));
    result = HDiffPatcher::apply(tool, base, patch, out, sha("old"), sha("new content"), 10, [] { return false; });
    QVERIFY(!result.ok()); QVERIFY(!QFile::exists(out));
}

void HDiffPatcherTest::cancelledOrFailedLeavesNoOutput()
{
    QTemporaryDir d; const auto base=d.filePath("base"), patch=d.filePath("patch"), out=d.filePath("out"), tool=d.filePath("hpatchz");
    put(base, "old"); put(patch, "new"); put(tool, "#!/bin/sh\nprintf broken > \"$4\"\nexit 1\n");
    QVERIFY(QFile::setPermissions(tool, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    auto result=HDiffPatcher::apply(tool,base,patch,out,sha("old"),sha("new"),3,[]{return false;});
    QVERIFY(!result.ok()); QVERIFY(!QFile::exists(out));
    result=HDiffPatcher::apply(tool,base,patch,out,sha("old"),sha("new"),3,[]{return true;});
    QCOMPARE(result.result, HDiffPatcher::Result::Cancelled); QVERIFY(!QFile::exists(out));
}

void HDiffPatcherTest::cancellationStopsDecoder()
{
    QTemporaryDir d; const auto base=d.filePath("base"), patch=d.filePath("patch"), out=d.filePath("out"), tool=d.filePath("hpatchz");
    put(base, "old"); put(patch, "new"); put(tool, "#!/bin/sh\nprintf broken > \"$4\"\nexec /bin/sleep 5\n");
    QVERIFY(QFile::setPermissions(tool, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    std::atomic<bool> cancel{false};
    std::thread trigger([&cancel] { std::this_thread::sleep_for(std::chrono::milliseconds(200)); cancel = true; });
    const auto result=HDiffPatcher::apply(tool,base,patch,out,sha("old"),sha("new"),20,[&cancel]{return cancel.load();});
    trigger.join();
    QCOMPARE(result.result, HDiffPatcher::Result::Cancelled); QVERIFY(!QFile::exists(out));
}

void HDiffPatcherTest::realPatchWhenAvailable()
{
    const QString encoder=qEnvironmentVariable("HDIFFZ"), decoder=qEnvironmentVariable("HPATCHZ");
    if (encoder.isEmpty() || decoder.isEmpty()) QSKIP("Set HDIFFZ and HPATCHZ for real decoder integration");
    QTemporaryDir d; const auto base=d.filePath("base"), target=d.filePath("target"), patch=d.filePath("patch"), out=d.filePath("out");
    const QByteArray a(50000, 'a'), b=QByteArray(25000,'a')+QByteArray(25000,'b'); put(base,a); put(target,b);
    QCOMPARE(QProcess::execute(encoder,{QStringLiteral("-c-zstd-20"),base,target,patch}), 0);
    const auto result=HDiffPatcher::apply(decoder,base,patch,out,sha(a),sha(b),b.size(),[]{return false;});
    QVERIFY2(result.ok(),qPrintable(result.error)); QFile f(out); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), b);
}

void HDiffPatcherTest::realReleaseWhenAvailable()
{
    const auto base=qEnvironmentVariable("ROUTING_BASE"), target=qEnvironmentVariable("ROUTING_TARGET");
    const auto patch=qEnvironmentVariable("ROUTING_PATCH"), decoder=qEnvironmentVariable("HPATCHZ");
    if (base.isEmpty() || target.isEmpty() || patch.isEmpty() || decoder.isEmpty()) QSKIP("Release fixtures not configured");
    auto fileHash=[](const QString &path) {
        QFile f(path); if (!f.open(QIODevice::ReadOnly)) return QString{};
        QCryptographicHash hash(QCryptographicHash::Sha256); if (!hash.addData(&f)) return QString{};
        return QString::fromLatin1(hash.result().toHex());
    };
    const auto baseHash=fileHash(base), targetHash=fileHash(target);
    QVERIFY(!baseHash.isEmpty()); QVERIFY(!targetHash.isEmpty());
    QTemporaryDir d; const auto out=d.filePath("routing.tar");
    const auto result=HDiffPatcher::apply(decoder,base,patch,out,baseHash,targetHash,QFileInfo(target).size(),[]{return false;});
    QVERIFY2(result.ok(),qPrintable(result.error)); QCOMPARE(fileHash(out),targetHash);
}

QTEST_GUILESS_MAIN(HDiffPatcherTest)
#include "HDiffPatcherTest.moc"

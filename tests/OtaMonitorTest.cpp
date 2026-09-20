#include <QtTest>

#include "l10n/Translations.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/OtaMonitor.h"
#include "services/ToastService.h"
#include "stores/OtaStore.h"

// OTA state transitions each announce once; progress ticks stay silent.
class OtaMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void downloadingAnnouncesVersion();
    void installingAnnouncesVersion();
    void pendingRebootIsInfo();
    void errorAnnouncesMessage();
    void cumulativeErrorsAreAllVisible();
    void simultaneousComponentErrorsAreBothShown();
    void progressTicksDoNotRepeat();
    void mdbSpeaksWhenDbcIdle();

private:
    struct Fixture {
        InMemoryMdbRepository repo;
        OtaStore ota{&repo};
        ToastService toast;
        Translations translations;

        void start() { ota.start(); }
    };

    static QStringList messages(const ToastService &toast);
    static QString typeOf(const ToastService &toast, int index);
    static void seedDownloading(Fixture &f);
};

QStringList OtaMonitorTest::messages(const ToastService &toast)
{
    QStringList out;
    for (const auto &entry : toast.toasts())
        out << entry.toMap().value(QStringLiteral("message")).toString();
    return out;
}

QString OtaMonitorTest::typeOf(const ToastService &toast, int index)
{
    return toast.toasts().at(index).toMap().value(QStringLiteral("type")).toString();
}

void OtaMonitorTest::seedDownloading(Fixture &f)
{
    f.repo.set(QStringLiteral("ota"), QStringLiteral("status:dbc"),
               QStringLiteral("downloading"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("update-version:dbc"),
               QStringLiteral("1.3.0"));
}

void OtaMonitorTest::downloadingAnnouncesVersion()
{
    Fixture f;
    f.start();
    OtaMonitor monitor(&f.ota, &f.toast, &f.translations);

    seedDownloading(f);

    QCOMPARE(f.toast.toasts().size(), 1);
    QCOMPARE(messages(f.toast).first(), QStringLiteral("Downloading Librescoot 1.3.0 update"));
    QCOMPARE(typeOf(f.toast, 0), QStringLiteral("info"));
}

void OtaMonitorTest::installingAnnouncesVersion()
{
    Fixture f;
    f.start();
    OtaMonitor monitor(&f.ota, &f.toast, &f.translations);

    seedDownloading(f);
    f.repo.set(QStringLiteral("ota"), QStringLiteral("status:dbc"),
               QStringLiteral("installing"));

    QCOMPARE(f.toast.toasts().size(), 2);
    QCOMPARE(messages(f.toast).last(), QStringLiteral("Installing Librescoot 1.3.0 update"));
    QCOMPARE(typeOf(f.toast, 1), QStringLiteral("info"));
}

void OtaMonitorTest::pendingRebootIsInfo()
{
    Fixture f;
    f.start();
    OtaMonitor monitor(&f.ota, &f.toast, &f.translations);

    seedDownloading(f);
    f.repo.set(QStringLiteral("ota"), QStringLiteral("status:dbc"),
               QStringLiteral("pending-reboot"));

    QCOMPARE(f.toast.toasts().size(), 2);
    QCOMPARE(messages(f.toast).last(),
             QStringLiteral("Update installed, will be applied on next boot."));
    QCOMPARE(typeOf(f.toast, 1), QStringLiteral("info"));
}

void OtaMonitorTest::errorAnnouncesMessage()
{
    Fixture f;
    f.start();
    OtaMonitor monitor(&f.ota, &f.toast, &f.translations);

    seedDownloading(f);
    f.repo.set(QStringLiteral("ota"), QStringLiteral("status:dbc"), QStringLiteral("error"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("error:dbc"), QStringLiteral("install-failed"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("error-message:dbc"),
               QStringLiteral("checksum mismatch"));

    QCOMPARE(f.toast.toasts().size(), 2);
    QCOMPARE(messages(f.toast).last(), QStringLiteral("Update failed: DBC: checksum mismatch"));
    QCOMPARE(typeOf(f.toast, 1), QStringLiteral("error"));
}

void OtaMonitorTest::cumulativeErrorsAreAllVisible()
{
    Fixture f;
    f.start();
    OtaMonitor monitor(&f.ota, &f.toast, &f.translations);

    f.repo.set(QStringLiteral("ota"), QStringLiteral("error-message:dbc"),
               QStringLiteral("full image download timed out"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("error-history:dbc"),
               QStringLiteral("delta checksum mismatch\nfull image download timed out"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("status:dbc"), QStringLiteral("error"));

    QCOMPARE(f.toast.toasts().size(), 1);
    QCOMPARE(messages(f.toast).first(),
             QStringLiteral("Update failed: DBC: delta checksum mismatch\nfull image download timed out"));
}

void OtaMonitorTest::simultaneousComponentErrorsAreBothShown()
{
    Fixture f;
    f.start();
    OtaMonitor monitor(&f.ota, &f.toast, &f.translations);

    f.repo.set(QStringLiteral("ota"), QStringLiteral("error-message:dbc"),
               QStringLiteral("network unavailable"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("status:dbc"), QStringLiteral("error"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("error-message:mdb"),
               QStringLiteral("network unavailable"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("status:mdb"), QStringLiteral("error"));

    QCOMPARE(f.toast.toasts().size(), 2);
    QCOMPARE(messages(f.toast).at(0), QStringLiteral("Update failed: DBC: network unavailable"));
    QCOMPARE(messages(f.toast).at(1), QStringLiteral("Update failed: MDB: network unavailable"));
}

void OtaMonitorTest::progressTicksDoNotRepeat()
{
    Fixture f;
    f.start();
    OtaMonitor monitor(&f.ota, &f.toast, &f.translations);

    seedDownloading(f);
    QCOMPARE(f.toast.toasts().size(), 1);

    f.repo.set(QStringLiteral("ota"), QStringLiteral("download-progress:dbc"), QStringLiteral("42"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("download-progress:dbc"), QStringLiteral("43"));
    QTest::qWait(50);
    QCOMPARE(f.toast.toasts().size(), 1);
}

void OtaMonitorTest::mdbSpeaksWhenDbcIdle()
{
    Fixture f;
    f.start();
    OtaMonitor monitor(&f.ota, &f.toast, &f.translations);

    f.repo.set(QStringLiteral("ota"), QStringLiteral("status:mdb"),
               QStringLiteral("downloading"));
    f.repo.set(QStringLiteral("ota"), QStringLiteral("update-version:mdb"),
               QStringLiteral("2.0.0"));

    QCOMPARE(f.toast.toasts().size(), 1);
    QCOMPARE(messages(f.toast).first(), QStringLiteral("Downloading Librescoot 2.0.0 update"));
}

QTEST_GUILESS_MAIN(OtaMonitorTest)
#include "OtaMonitorTest.moc"

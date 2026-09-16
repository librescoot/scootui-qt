#include <QtTest>

#include "l10n/Translations.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/LowSocMonitor.h"
#include "services/ToastService.h"
#include "stores/BatteryStore.h"

// Downward SoC crossings for the main pack produce rider warnings; the first
// observed value is only a baseline.
class LowSocMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void warnsOnEachDownwardCrossing();
    void noWarningAtBootWithLowBattery();
    void noWarningWhenSocRises();

private:
    struct Fixture {
        InMemoryMdbRepository repo;
        BatteryStore battery0{&repo, QStringLiteral("0")};
        ToastService toast;
        Translations translations;

        void start() { battery0.start(); }
    };

    static void seed(Fixture &f, int charge, bool present = true);
};

void LowSocMonitorTest::seed(Fixture &f, int charge, bool present)
{
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("present"),
               present ? QStringLiteral("true") : QStringLiteral("false"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"),
               QString::number(charge), false);
}

void LowSocMonitorTest::warnsOnEachDownwardCrossing()
{
    Fixture f;
    seed(f, 30);
    f.start();

    LowSocMonitor monitor(&f.battery0, &f.toast, &f.translations);

    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("19"));
    QCOMPARE(f.toast.toasts().size(), 1); // <20

    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("9"));
    QCOMPARE(f.toast.toasts().size(), 2); // <=10

    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("4"));
    QCOMPARE(f.toast.toasts().size(), 3); // <5

    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("0"));
    QCOMPARE(f.toast.toasts().size(), 4); // empty
}

void LowSocMonitorTest::noWarningAtBootWithLowBattery()
{
    Fixture f;
    seed(f, 3);
    f.start();

    LowSocMonitor monitor(&f.battery0, &f.toast, &f.translations);

    // No crossing yet: 3 was the baseline.
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("2"));
    QTest::qWait(50);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void LowSocMonitorTest::noWarningWhenSocRises()
{
    Fixture f;
    seed(f, 12);
    f.start();

    LowSocMonitor monitor(&f.battery0, &f.toast, &f.translations);

    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("60"));
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("90"));
    QTest::qWait(50);
    QCOMPARE(f.toast.toasts().size(), 0);
}

QTEST_GUILESS_MAIN(LowSocMonitorTest)
#include "LowSocMonitorTest.moc"

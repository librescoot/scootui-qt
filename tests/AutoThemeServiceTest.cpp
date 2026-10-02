#include <QtTest>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "repositories/InMemoryMdbRepository.h"
#include "services/AutoThemeService.h"
#include "stores/SettingsStore.h"
#include "stores/ThemeStore.h"

class AutoThemeServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void localDarkWinsOverStaleRedis();
    void localLightSeedsTheme();
    void startupSampleAppliesWithoutEventLoop();
    void localLightWinsOverStaleRedis();
    void reentrySamplesCurrentLight();
    void manualBootRequestsFreshSample_data();
    void manualBootRequestsFreshSample();
    void inFlightBootSampleCannotSeedReentry();
    void lateSampleAfterDisableIsIgnored();
    void explicitThemeDisablesSensor();
    void invalidOrMissingSensorFallsBack();
    void invalidReentryReturnsToRedis();
    void pubsubWaitsForFreshBrightness();
    void slowSensorDoesNotBlockEnable();
    void slowSensorFallsBackAtDeadline();
};

class DelayedFieldRepository : public InMemoryMdbRepository
{
public:
    using InMemoryMdbRepository::InMemoryMdbRepository;

    void requestField(const QString &channel, const QString &field) override
    {
        m_channel = channel;
        m_field = field;
        ++m_requests;
    }

    int requests() const { return m_requests; }

    void completeField(const QString &value)
    {
        set(m_channel, m_field, value, false);
        emit fieldFetched(m_channel, m_field, value);
    }

private:
    QString m_channel;
    QString m_field;
    int m_requests = 0;
};

static std::unique_ptr<AutoThemeService> serviceFor(InMemoryMdbRepository &repo,
                                                     SettingsStore &settings,
                                                     ThemeStore &theme,
                                                     AmbientLightProbe &probe)
{
    settings.start();
    auto service = std::make_unique<AutoThemeService>(&repo, &theme);
    service->setLocalProbe(&probe);
    service->setEnabled(true);
    return service;
}

void AutoThemeServiceTest::localDarkWinsOverStaleRedis()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    repo.set("dashboard", "brightness", "100", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) { return QStringLiteral("1"); });
    auto service = serviceFor(repo, settings, theme, probe);
    QTRY_VERIFY_WITH_TIMEOUT(probe.finished(), 500);
    QTRY_VERIFY_WITH_TIMEOUT(theme.isDark(), 500);
    QTest::qWait(10500);
    QVERIFY(theme.isDark());
}

void AutoThemeServiceTest::localLightSeedsTheme()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    repo.set("dashboard", "brightness", "1", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) { return QStringLiteral("100"); });
    auto service = serviceFor(repo, settings, theme, probe);
    QTRY_VERIFY_WITH_TIMEOUT(!theme.isDark(), 500);
}

void AutoThemeServiceTest::startupSampleAppliesWithoutEventLoop()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    repo.set("dashboard", "brightness", "0", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return QStringLiteral("100");
    });
    auto service = serviceFor(repo, settings, theme, probe);
    QElapsedTimer deadline;
    deadline.start();
    while (!probe.finished() && deadline.elapsed() < 1000)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    QVERIFY(probe.finished());
    QVERIFY(theme.isDark());
    service->applyStartupSample();
    QVERIFY(!theme.isDark());
}

void AutoThemeServiceTest::localLightWinsOverStaleRedis()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    repo.set("dashboard", "brightness", "1", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) { return QStringLiteral("100"); });
    auto service = serviceFor(repo, settings, theme, probe);
    QTRY_VERIFY_WITH_TIMEOUT(probe.finished(), 500);
    QTRY_VERIFY_WITH_TIMEOUT(!theme.isDark(), 500);
    QTest::qWait(10500);
    QVERIFY(!theme.isDark());
}

void AutoThemeServiceTest::reentrySamplesCurrentLight()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    std::atomic<int> reads{0};
    AmbientLightProbe probe({}, [&reads](const QString &) {
        return reads.fetch_add(1) == 0 ? QStringLiteral("1") : QStringLiteral("100");
    });
    auto service = serviceFor(repo, settings, theme, probe);
    QTRY_VERIFY_WITH_TIMEOUT(probe.finished(), 500);
    QTRY_VERIFY_WITH_TIMEOUT(theme.isDark(), 500);
    service->setEnabled(false);
    theme.setTheme(QStringLiteral("light"));
    service->setEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(reads.load() >= 2, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!theme.isDark(), 500);
}

void AutoThemeServiceTest::manualBootRequestsFreshSample_data()
{
    QTest::addColumn<QString>("manualTheme");
    QTest::addColumn<QString>("bootLux");
    QTest::addColumn<QString>("currentLux");
    QTest::addColumn<bool>("expectedDark");
    QTest::newRow("dark-to-current-light") << QString("dark") << QString("1")
        << QString("100") << false;
    QTest::newRow("light-to-current-dark") << QString("light") << QString("100")
        << QString("1") << true;
}

void AutoThemeServiceTest::manualBootRequestsFreshSample()
{
    QFETCH(QString, manualTheme);
    QFETCH(QString, bootLux);
    QFETCH(QString, currentLux);
    QFETCH(bool, expectedDark);
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", manualTheme, false);
    repo.set("dashboard", "brightness", bootLux, false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    std::atomic<bool> changed{false};
    std::atomic<int> reads{0};
    AmbientLightProbe probe({}, [&](const QString &) {
        ++reads;
        return changed.load() ? currentLux : bootLux;
    });
    probe.start();
    QTRY_VERIFY_WITH_TIMEOUT(probe.finished(), 500);
    auto service = serviceFor(repo, settings, theme, probe);
    QVERIFY(!service->isEnabled());
    changed.store(true);
    repo.set("settings", "dashboard.theme", "auto");
    QTRY_VERIFY_WITH_TIMEOUT(theme.isAutoMode(), 500);
    service->setEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(reads.load() >= 2, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(theme.isDark(), expectedDark, 500);
}

void AutoThemeServiceTest::inFlightBootSampleCannotSeedReentry()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "light", false);
    repo.set("dashboard", "brightness", "100", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    std::atomic<int> reads{0};
    std::atomic<bool> releaseBootRead{false};
    AmbientLightProbe probe({}, [&](const QString &) {
        if (reads.fetch_add(1) == 0) {
            const auto deadline = std::chrono::steady_clock::now()
                + std::chrono::seconds(2);
            while (!releaseBootRead.load() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return QStringLiteral("100");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        return QStringLiteral("1");
    });
    probe.start();
    QTRY_COMPARE_WITH_TIMEOUT(reads.load(), 1, 500);
    auto service = serviceFor(repo, settings, theme, probe);
    repo.set("settings", "dashboard.theme", "auto");
    QTRY_VERIFY_WITH_TIMEOUT(theme.isAutoMode(), 500);
    service->setEnabled(true);
    releaseBootRead.store(true);
    QTRY_VERIFY_WITH_TIMEOUT(probe.finished() && probe.generation() >= 2, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(theme.isDark(), 500);
}

void AutoThemeServiceTest::lateSampleAfterDisableIsIgnored()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return QStringLiteral("1");
    });
    auto service = serviceFor(repo, settings, theme, probe);
    service->setEnabled(false);
    theme.setTheme(QStringLiteral("light"));
    QTest::qWait(400);
    QVERIFY(!theme.isDark());
}

void AutoThemeServiceTest::explicitThemeDisablesSensor()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "light", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) { return QStringLiteral("1"); });
    settings.start();
    auto service = std::make_unique<AutoThemeService>(&repo, &theme);
    service->setLocalProbe(&probe);
    QTest::qWait(100);
    QVERIFY(!theme.isAutoMode());
    QVERIFY(!theme.isDark());
}

void AutoThemeServiceTest::invalidOrMissingSensorFallsBack()
{
    for (const QString &value : {QString(), QStringLiteral("not-a-number")}) {
        InMemoryMdbRepository repo;
        repo.set("settings", "dashboard.theme", "auto", false);
        repo.set("dashboard", "brightness", "100", false);
        SettingsStore settings(&repo);
        ThemeStore theme(&settings);
        AmbientLightProbe probe({}, [value](const QString &) { return value; });
        auto service = serviceFor(repo, settings, theme, probe);
        QTRY_VERIFY_WITH_TIMEOUT(probe.finished(), 500);
        QTRY_VERIFY_WITH_TIMEOUT(!theme.isDark(), 500);
    }
}

void AutoThemeServiceTest::invalidReentryReturnsToRedis()
{
    for (const QString &value : {QString(), QStringLiteral("not-a-number")}) {
        InMemoryMdbRepository repo;
        repo.set("settings", "dashboard.theme", "auto", false);
        repo.set("dashboard", "brightness", "100", false);
        SettingsStore settings(&repo);
        ThemeStore theme(&settings);
        AmbientLightProbe probe({}, [value](const QString &) { return value; });
        auto service = serviceFor(repo, settings, theme, probe);
        QTRY_VERIFY_WITH_TIMEOUT(!theme.isDark(), 500);
        service->setEnabled(false);
        theme.setTheme(QStringLiteral("dark"));
        service->setEnabled(true);
        QTRY_VERIFY_WITH_TIMEOUT(!theme.isDark(), 1500);
    }
}

void AutoThemeServiceTest::pubsubWaitsForFreshBrightness()
{
    DelayedFieldRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    repo.set("dashboard", "brightness", "1", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) { return QStringLiteral("100"); });
    auto service = serviceFor(repo, settings, theme, probe);
    QTRY_VERIFY_WITH_TIMEOUT(!theme.isDark(), 500);

    repo.publish("dashboard", "brightness");
    QTRY_COMPARE_WITH_TIMEOUT(repo.requests(), 1, 500);
    QTest::qWait(10500);
    QVERIFY(!theme.isDark());

    repo.completeField(QStringLiteral("1"));
    QTRY_VERIFY_WITH_TIMEOUT(theme.isDark(), 3500);
}

void AutoThemeServiceTest::slowSensorDoesNotBlockEnable()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    repo.set("dashboard", "brightness", "100", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return QStringLiteral("1");
    });
    const auto started = std::chrono::steady_clock::now();
    auto service = serviceFor(repo, settings, theme, probe);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    QVERIFY(elapsed < 50);
    QVERIFY(theme.isDark());
    QTest::qWait(50);
    QVERIFY(theme.isDark());
    QTRY_VERIFY_WITH_TIMEOUT(probe.finished(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(theme.isDark(), 500);
    QTest::qWait(3000);
    QVERIFY(theme.isDark());
    service.reset();
}

void AutoThemeServiceTest::slowSensorFallsBackAtDeadline()
{
    InMemoryMdbRepository repo;
    repo.set("settings", "dashboard.theme", "auto", false);
    repo.set("dashboard", "brightness", "100", false);
    SettingsStore settings(&repo);
    ThemeStore theme(&settings);
    AmbientLightProbe probe({}, [](const QString &) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        return QStringLiteral("1");
    });
    QElapsedTimer elapsed;
    elapsed.start();
    qint64 fallbackAt = -1;
    connect(&theme, &ThemeStore::themeChanged, this, [&]() {
        if (!theme.isDark() && fallbackAt < 0)
            fallbackAt = elapsed.elapsed();
    });
    auto service = serviceFor(repo, settings, theme, probe);
    QVERIFY(theme.isDark());
    QTRY_VERIFY_WITH_TIMEOUT(!theme.isDark(), 1600);
    QVERIFY2(fallbackAt >= 1200 && fallbackAt < 1750,
             qPrintable(QStringLiteral("Fallback at %1ms").arg(fallbackAt)));
    QTRY_VERIFY_WITH_TIMEOUT(probe.finished(), 4000);
    QTRY_VERIFY_WITH_TIMEOUT(theme.isDark(), 1500);
}

QTEST_MAIN(AutoThemeServiceTest)
#include "AutoThemeServiceTest.moc"

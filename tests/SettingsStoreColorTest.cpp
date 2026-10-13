#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "stores/SettingsStore.h"

// The speedometer fill colours are free-form hex in settings.toml, so the
// store has to be the thing that rejects a value the arc cannot render. These
// cover the parse guard and the fallback to the shipped ramp.
class SettingsStoreColorTest : public QObject
{
    Q_OBJECT

private slots:
    void unsetFallsBackToShippedColors();
    void additionalColors_data();
    void additionalColors();
    void speedStopsApplyAndFallBack();
    void acceptsRgbAndArgbHex();
    void rejectsMalformedValues();
    void runtimeUpdateAppliesAndFallsBack();
    void malformedRuntimeValueDoesNotBreakTheArc();
};

void SettingsStoreColorTest::unsetFallsBackToShippedColors()
{
    InMemoryMdbRepository repo;
    SettingsStore store(&repo);
    store.start();

    QCOMPARE(store.speedometerOriginColor(), QStringLiteral("#90CAF9"));
    QCOMPARE(store.speedometerBaseColor(), QStringLiteral("#2196F3"));
    QCOMPARE(store.speedometerWarnColor(), QStringLiteral("#9C27B0"));
    QCOMPARE(store.speedometerOverspeedColor(), QStringLiteral("#E91E63"));
}

void SettingsStoreColorTest::additionalColors_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QByteArray>("property");
    QTest::addColumn<QString>("fallback");
    QTest::newRow("regen-arc") << QStringLiteral("dashboard.speedometer.regen-color")
        << QByteArray("speedometerRegenColor") << QStringLiteral("#4DFF0000");
    QTest::newRow("error-arc") << QStringLiteral("dashboard.speedometer.error-color")
        << QByteArray("speedometerErrorColor") << QStringLiteral("#F44336");
    QTest::newRow("regen-bar") << QStringLiteral("dashboard.power.regen-color")
        << QByteArray("powerRegenColor") << QStringLiteral("#43A047");
    QTest::newRow("discharge-bar") << QStringLiteral("dashboard.power.discharge-color")
        << QByteArray("powerDischargeColor") << QStringLiteral("#1E88E5");
    QTest::newRow("boost-bar") << QStringLiteral("dashboard.power.boost-color")
        << QByteArray("powerBoostColor") << QStringLiteral("#FB8C00");
}

void SettingsStoreColorTest::additionalColors()
{
    QFETCH(QString, key);
    QFETCH(QByteArray, property);
    QFETCH(QString, fallback);

    InMemoryMdbRepository repo;
    SettingsStore defaults(&repo);
    defaults.start();
    QCOMPARE(defaults.property(property.constData()).toString(), fallback);

    repo.set(QStringLiteral("settings"), key, QStringLiteral("#80123456"), false);
    SettingsStore store(&repo);
    store.start();
    QCOMPARE(store.property(property.constData()).toString(), QStringLiteral("#80123456"));

    const auto metaProperty = store.metaObject()->property(
        store.metaObject()->indexOfProperty(property.constData()));
    QSignalSpy changed(&store, metaProperty.notifySignal());
    QVERIFY(changed.isValid());
    repo.set(QStringLiteral("settings"), key, QStringLiteral("#abcdef"));
    QCOMPARE(store.property(property.constData()).toString(), QStringLiteral("#abcdef"));
    QCOMPARE(changed.count(), 1);
    repo.set(QStringLiteral("settings"), key, QStringLiteral("invalid"));
    QCOMPARE(store.property(property.constData()).toString(), fallback);
    QCOMPARE(changed.count(), 2);
    repo.set(QStringLiteral("settings"), key, QString());
    QCOMPARE(store.property(property.constData()).toString(), fallback);
    QCOMPARE(changed.count(), 3);
}

void SettingsStoreColorTest::speedStopsApplyAndFallBack()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.normal-speed"),
             QStringLiteral("35"), false);

    SettingsStore store(&repo);
    store.start();

    QCOMPARE(store.speedometerNormalSpeed(), 35);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.normal-speed"),
             QStringLiteral("invalid"));
    QCOMPARE(store.speedometerNormalSpeed(), 30);
}

void SettingsStoreColorTest::acceptsRgbAndArgbHex()
{
    QVERIFY(SettingsStore::isValidHexColor(QStringLiteral("#FFDE21")));
    QVERIFY(SettingsStore::isValidHexColor(QStringLiteral("#ffde21")));
    QVERIFY(SettingsStore::isValidHexColor(QStringLiteral("#80FFDE21")));
    QVERIFY(SettingsStore::isValidHexColor(QStringLiteral("#000000")));
    QVERIFY(SettingsStore::isValidHexColor(QStringLiteral("#FFFFFFFF")));
}

void SettingsStoreColorTest::rejectsMalformedValues()
{
    QVERIFY(!SettingsStore::isValidHexColor(QString()));
    QVERIFY(!SettingsStore::isValidHexColor(QStringLiteral("FFDE21")));      // no hash
    QVERIFY(!SettingsStore::isValidHexColor(QStringLiteral("#FFDE2")));      // too short
    QVERIFY(!SettingsStore::isValidHexColor(QStringLiteral("#FFDE211")));    // 5-digit alpha
    QVERIFY(!SettingsStore::isValidHexColor(QStringLiteral("#FFDE21FF0")));  // too long
    QVERIFY(!SettingsStore::isValidHexColor(QStringLiteral("#GGGGGG")));     // not hex
    QVERIFY(!SettingsStore::isValidHexColor(QStringLiteral("blue")));        // named colour
    QVERIFY(!SettingsStore::isValidHexColor(QStringLiteral("#FFDE21 ")));    // trailing space
}

void SettingsStoreColorTest::runtimeUpdateAppliesAndFallsBack()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.origin-color"),
             QStringLiteral("#010203"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.base-color"),
             QStringLiteral("#111111"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.warn-color"),
             QStringLiteral("#FFDE21"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.overspeed-color"),
             QStringLiteral("#80E91E63"), false);

    SettingsStore store(&repo);
    store.start();

    QCOMPARE(store.speedometerOriginColor(), QStringLiteral("#010203"));
    QCOMPARE(store.speedometerBaseColor(), QStringLiteral("#111111"));
    QCOMPARE(store.speedometerWarnColor(), QStringLiteral("#FFDE21"));
    QCOMPARE(store.speedometerOverspeedColor(), QStringLiteral("#80E91E63"));

    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.warn-color"),
             QStringLiteral("#00A0FF"));
    QCOMPARE(store.speedometerWarnColor(), QStringLiteral("#00A0FF"));
}

void SettingsStoreColorTest::malformedRuntimeValueDoesNotBreakTheArc()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.warn-color"),
             QStringLiteral("purple"), false);

    SettingsStore store(&repo);
    store.start();

    QCOMPARE(store.speedometerWarnColor(), QStringLiteral("#9C27B0"));

    // A live update to garbage leaves the shipped colour in place rather than
    // handing QML a string it cannot coerce to a color.
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.speedometer.base-color"),
             QStringLiteral("#12345"));
    QCOMPARE(store.speedometerBaseColor(), QStringLiteral("#2196F3"));
}

QTEST_MAIN(SettingsStoreColorTest)
#include "SettingsStoreColorTest.moc"

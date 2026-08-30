#include <QtTest>

#include "services/BatteryAlertPolicy.h"

using namespace BatteryAlertPolicy;

namespace {

// A healthy riding baseline: main battery in and active, CBB fine, AUX fine,
// seatbox closed. Individual tests knock one thing over.
Inputs baseline()
{
    Inputs in;
    in.main0Present = true;
    in.main0Charge = 80;
    in.main0State = static_cast<int>(ScootEnums::BatteryState::Active);
    in.main1Present = false;
    in.cbPresent = true;
    in.cbChargeValid = true;
    in.cbCharge = 90;
    in.cbChargeStatus = static_cast<int>(ScootEnums::ChargeStatus::Charging);
    in.auxVoltageValid = true;
    in.auxVoltageMv = 12500;
    in.auxChargeStatus = static_cast<int>(ScootEnums::AuxChargeStatus::FloatCharge);
    in.seatboxClosed = true;
    return in;
}

}

class BatteryAlertPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void baselineRaisesNothing();
    void cbWarningNeedsDischargeAndActiveMain();
    void auxWarningTiers();
    void auxCriticalIgnoresChargeStatus();
    void openSeatboxSuppressesChargingWarnings();
    void strandedNeedsNoMainBattery();
    void strandedIgnoresSeatbox();
    void lowReadsRequireValidData();
    void rangeModel();
    void valueTextFormatting();
};

void BatteryAlertPolicyTest::baselineRaisesNothing()
{
    const Inputs in = baseline();
    QVERIFY(!cbWarning(in));
    QVERIFY(!auxWarning(in));
    QVERIFY(!cbStranded(in));
    QVERIFY(!auxStranded(in));
    QVERIFY(!cbLow(in));
    QVERIFY(!auxLow(in));
}

void BatteryAlertPolicyTest::cbWarningNeedsDischargeAndActiveMain()
{
    Inputs in = baseline();
    in.cbCharge = 40;
    in.cbChargeStatus = static_cast<int>(ScootEnums::ChargeStatus::NotCharging);
    QVERIFY(cbWarning(in));

    // Still charging: the system is doing its job, no warning.
    in.cbChargeStatus = static_cast<int>(ScootEnums::ChargeStatus::Charging);
    QVERIFY(!cbWarning(in));

    // Main battery idle rather than active: not a charging-system fault.
    in.cbChargeStatus = static_cast<int>(ScootEnums::ChargeStatus::NotCharging);
    in.main0State = static_cast<int>(ScootEnums::BatteryState::Idle);
    QVERIFY(!cbWarning(in));

    // Empty main battery can't charge anything.
    in.main0State = static_cast<int>(ScootEnums::BatteryState::Active);
    in.main0Charge = 0;
    QVERIFY(!cbWarning(in));
}

void BatteryAlertPolicyTest::auxWarningTiers()
{
    Inputs in = baseline();
    in.auxChargeStatus = static_cast<int>(ScootEnums::AuxChargeStatus::NotCharging);

    in.auxVoltageMv = kAuxWarnVoltageMv;
    QVERIFY(!auxWarning(in));

    in.auxVoltageMv = kAuxWarnVoltageMv - 1;
    QVERIFY(auxWarning(in));

    // 11700 is only the soft "low" line, not a warning.
    in.auxVoltageMv = kAuxLowVoltageMv - 1;
    QVERIFY(auxLow(in));
    QVERIFY(!auxWarning(in));
}

void BatteryAlertPolicyTest::auxCriticalIgnoresChargeStatus()
{
    Inputs in = baseline();
    // Below critical the charger state doesn't matter: it is plainly failing.
    in.auxVoltageMv = kAuxCriticalVoltageMv - 1;
    in.auxChargeStatus = static_cast<int>(ScootEnums::AuxChargeStatus::BulkCharge);
    QVERIFY(auxWarning(in));
}

void BatteryAlertPolicyTest::openSeatboxSuppressesChargingWarnings()
{
    Inputs in = baseline();
    in.cbCharge = 40;
    in.cbChargeStatus = static_cast<int>(ScootEnums::ChargeStatus::NotCharging);
    in.auxVoltageMv = kAuxCriticalVoltageMv - 1;
    in.seatboxClosed = false;
    QVERIFY(!cbWarning(in));
    QVERIFY(!auxWarning(in));
}

void BatteryAlertPolicyTest::strandedNeedsNoMainBattery()
{
    Inputs in = baseline();
    in.cbCharge = 40;
    in.auxVoltageMv = kAuxLowVoltageMv - 1;
    // Main battery present: stranded warnings stay quiet.
    QVERIFY(!cbStranded(in));
    QVERIFY(!auxStranded(in));

    in.main0Present = false;
    QVERIFY(cbStranded(in));
    QVERIFY(auxStranded(in));

    // A pack in the second slot counts as a main battery too.
    in.main1Present = true;
    QVERIFY(!cbStranded(in));
    QVERIFY(!auxStranded(in));
}

void BatteryAlertPolicyTest::strandedIgnoresSeatbox()
{
    Inputs in = baseline();
    in.main0Present = false;
    in.cbCharge = 40;
    in.auxVoltageMv = kAuxLowVoltageMv - 1;
    in.seatboxClosed = false;
    QVERIFY(cbStranded(in));
    QVERIFY(auxStranded(in));
}

void BatteryAlertPolicyTest::lowReadsRequireValidData()
{
    Inputs in = baseline();
    in.cbCharge = 0;
    in.cbChargeValid = false;
    in.auxVoltageMv = 0;
    in.auxVoltageValid = false;
    QVERIFY(!cbLow(in));
    QVERIFY(!auxLow(in));
    in.main0Present = false;
    QVERIFY(!cbStranded(in));
    QVERIFY(!auxStranded(in));
}

void BatteryAlertPolicyTest::rangeModel()
{
    QCOMPARE(rangeKm(100, 100), 45.0);
    QCOMPARE(rangeKm(100, 50), 22.5);
    QCOMPARE(rangeKm(80, 50), 18.0);
    QCOMPARE(rangeKm(100, 0), 0.0);
}

void BatteryAlertPolicyTest::valueTextFormatting()
{
    // Percentage mode passes the charge straight through.
    QCOMPARE(valueText(73, 100, false, true), QStringLiteral("73"));

    // Range >= 10 km drops decimals even when allowed.
    QCOMPARE(valueText(50, 100, true, true), QStringLiteral("22"));

    // Small range keeps one decimal only while decimals are allowed.
    QCOMPARE(valueText(20, 100, true, true), QStringLiteral("9.0"));
    QCOMPARE(valueText(22, 100, true, true), QStringLiteral("9.9"));
    QCOMPARE(valueText(22, 100, true, false), QStringLiteral("9"));
}

QTEST_MAIN(BatteryAlertPolicyTest)
#include "BatteryAlertPolicyTest.moc"

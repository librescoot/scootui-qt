#include <QtTest>
#include <QMetaEnum>

#include "models/EnumStrings.h"
#include "models/Enums.h"

// Every enumerator has to come back from its own wire string, so a value added
// to an enum without a string, or a string the parser does not accept, fails
// here rather than showing up as an int on the debug screen.
class EnumStringsTest : public QObject
{
    Q_OBJECT

private slots:
    void toggle();
    void blinkerState();
    void blinkerSwitch();
    void vehicleState();
    void kickstand();
    void handleBarLockSensor();
    void seatboxLock();
    void batteryState();
    void gpsState();
    void modemState();
    void connectionStatus();
    void chargeStatus();
    void auxChargeStatus();
    void outOfRangeShowsNumber();

private:
    template <typename E, typename ToString, typename Parse>
    void roundTrip(ToString toString, Parse parse)
    {
        const QMetaEnum me = QMetaEnum::fromType<E>();
        QVERIFY(me.keyCount() > 0);
        for (int i = 0; i < me.keyCount(); ++i) {
            const auto v = static_cast<E>(me.value(i));
            const QString s = toString(v);
            QVERIFY2(!s.isEmpty(), me.key(i));
            QVERIFY2(parse(s) == v, qPrintable(QString::fromLatin1(me.key(i)) + " <- " + s));
        }
    }
};

void EnumStringsTest::toggle()
{
    roundTrip<ScootEnums::Toggle>(ScootEnums::toggleString, ScootEnums::parseToggle);
    QCOMPARE(EnumStrings().toggle(static_cast<int>(ScootEnums::Toggle::On)), QStringLiteral("on"));
}

void EnumStringsTest::blinkerState()
{
    roundTrip<ScootEnums::BlinkerState>(ScootEnums::blinkerStateString, ScootEnums::parseBlinkerState);
    QCOMPARE(EnumStrings().blinkerState(static_cast<int>(ScootEnums::BlinkerState::Both)), QStringLiteral("both"));
}

void EnumStringsTest::blinkerSwitch()
{
    roundTrip<ScootEnums::BlinkerSwitch>(ScootEnums::blinkerSwitchString, ScootEnums::parseBlinkerSwitch);
}

void EnumStringsTest::vehicleState()
{
    roundTrip<ScootEnums::VehicleState>(ScootEnums::vehicleStateString, ScootEnums::parseVehicleState);
}

void EnumStringsTest::kickstand()
{
    roundTrip<ScootEnums::Kickstand>(ScootEnums::kickstandString, ScootEnums::parseKickstand);
}

void EnumStringsTest::handleBarLockSensor()
{
    roundTrip<ScootEnums::HandleBarLockSensor>(ScootEnums::handleBarLockSensorString,
                                               ScootEnums::parseHandleBarLockSensor);
}

void EnumStringsTest::seatboxLock()
{
    roundTrip<ScootEnums::SeatboxLock>(ScootEnums::seatboxLockString, ScootEnums::parseSeatboxLock);
}

void EnumStringsTest::batteryState()
{
    roundTrip<ScootEnums::BatteryState>(ScootEnums::batteryStateString, ScootEnums::parseBatteryState);
}

void EnumStringsTest::gpsState()
{
    roundTrip<ScootEnums::GpsState>(ScootEnums::gpsStateString, ScootEnums::parseGpsState);
}

void EnumStringsTest::modemState()
{
    roundTrip<ScootEnums::ModemState>(ScootEnums::modemStateString, ScootEnums::parseModemState);
}

void EnumStringsTest::connectionStatus()
{
    roundTrip<ScootEnums::ConnectionStatus>(ScootEnums::connectionStatusString,
                                            ScootEnums::parseConnectionStatus);
}

void EnumStringsTest::chargeStatus()
{
    roundTrip<ScootEnums::ChargeStatus>(ScootEnums::chargeStatusString, ScootEnums::parseChargeStatus);
}

void EnumStringsTest::auxChargeStatus()
{
    roundTrip<ScootEnums::AuxChargeStatus>(ScootEnums::auxChargeStatusString,
                                           ScootEnums::parseAuxChargeStatus);
}

void EnumStringsTest::outOfRangeShowsNumber()
{
    EnumStrings e;
    QCOMPARE(e.blinkerState(42), QStringLiteral("42"));
    QCOMPARE(e.toggle(-1), QStringLiteral("-1"));
}

QTEST_APPLESS_MAIN(EnumStringsTest)
#include "EnumStringsTest.moc"

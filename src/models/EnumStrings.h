#pragma once

#include <QObject>
#include <QString>

// QML-side access to the enum wire strings. Enum-typed store properties reach
// QML as plain ints, so a screen that wants to show what Redis actually says
// has to go back through here.
class EnumStrings : public QObject
{
    Q_OBJECT
public:
    explicit EnumStrings(QObject *parent = nullptr);

    Q_INVOKABLE QString toggle(int v) const;
    Q_INVOKABLE QString blinkerState(int v) const;
    Q_INVOKABLE QString blinkerSwitch(int v) const;
    Q_INVOKABLE QString vehicleState(int v) const;
    Q_INVOKABLE QString kickstand(int v) const;
    Q_INVOKABLE QString handleBarLockSensor(int v) const;
    Q_INVOKABLE QString seatboxLock(int v) const;
    Q_INVOKABLE QString batteryState(int v) const;
    Q_INVOKABLE QString gpsState(int v) const;
    Q_INVOKABLE QString modemState(int v) const;
    Q_INVOKABLE QString connectionStatus(int v) const;
    Q_INVOKABLE QString chargeStatus(int v) const;
    Q_INVOKABLE QString auxChargeStatus(int v) const;
};

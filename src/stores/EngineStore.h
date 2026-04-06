#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class EngineStore : public SyncableStore
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(int powerState READ powerState NOTIFY powerStateChanged)
    Q_PROPERTY(int kers READ kers NOTIFY kersChanged)
    Q_PROPERTY(QString kersReasonOff READ kersReasonOff NOTIFY kersReasonOffChanged)
    Q_PROPERTY(double motorVoltage READ motorVoltage NOTIFY motorVoltageChanged)
    Q_PROPERTY(double motorCurrent READ motorCurrent NOTIFY motorCurrentChanged)
    Q_PROPERTY(double rpm READ rpm NOTIFY rpmChanged)
    Q_PROPERTY(double speed READ speed NOTIFY speedChanged)
    Q_PROPERTY(double rawSpeed READ rawSpeed NOTIFY rawSpeedChanged)
    Q_PROPERTY(bool hasRawSpeed READ hasRawSpeed NOTIFY rawSpeedChanged)
    Q_PROPERTY(int throttle READ throttle NOTIFY throttleChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY firmwareVersionChanged)
    Q_PROPERTY(double odometer READ odometer NOTIFY odometerChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)

public:
    explicit EngineStore(MdbRepository *repo, QObject *parent = nullptr);
    static EngineStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    int powerState() const { return static_cast<int>(m_powerState); }
    int kers() const { return static_cast<int>(m_kers); }
    QString kersReasonOff() const { return m_kersReasonOff; }
    double motorVoltage() const { return m_motorVoltage; }
    double motorCurrent() const { return m_motorCurrent; }
    double rpm() const { return m_rpm; }
    double speed() const { return m_speed; }
    double rawSpeed() const { return m_rawSpeed; }
    bool hasRawSpeed() const { return m_hasRawSpeed; }
    int throttle() const { return static_cast<int>(m_throttle); }
    QString firmwareVersion() const { return m_firmwareVersion; }
    double odometer() const { return m_odometer; }
    double temperature() const { return m_temperature; }

signals:
    void powerStateChanged();
    void kersChanged();
    void kersReasonOffChanged();
    void motorVoltageChanged();
    void motorCurrentChanged();
    void rpmChanged();
    void speedChanged();
    void rawSpeedChanged();
    void throttleChanged();
    void firmwareVersionChanged();
    void odometerChanged();
    void temperatureChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    ScootEnums::Toggle m_powerState = ScootEnums::Toggle::Off;
    ScootEnums::Toggle m_kers = ScootEnums::Toggle::On;
    QString m_kersReasonOff;
    double m_motorVoltage = 0;
    double m_motorCurrent = 0;
    double m_rpm = 0;
    double m_speed = 0;
    double m_rawSpeed = 0;
    bool m_hasRawSpeed = false;
    ScootEnums::Toggle m_throttle = ScootEnums::Toggle::Off;
    QString m_firmwareVersion;
    double m_odometer = 0;
    double m_temperature = 0;
    static inline EngineStore *s_instance = nullptr;
};

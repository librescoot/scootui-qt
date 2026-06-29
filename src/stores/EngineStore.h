#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"

#include <QSet>

class EngineStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(int kers READ kers NOTIFY kersChanged)
    Q_PROPERTY(QString kersReasonOff READ kersReasonOff NOTIFY kersReasonOffChanged)
    Q_PROPERTY(double motorVoltage READ motorVoltage NOTIFY motorVoltageChanged)
    Q_PROPERTY(double motorCurrent READ motorCurrent NOTIFY motorCurrentChanged)
    // EBS regen caps the ECU accepted (mV / mA), distinct from the commanded
    // KERS setpoint (reflects the ECU's clamp of what we requested).
    Q_PROPERTY(double acceptedRegenVoltage READ acceptedRegenVoltage NOTIFY acceptedRegenVoltageChanged)
    Q_PROPERTY(double acceptedRegenCurrent READ acceptedRegenCurrent NOTIFY acceptedRegenCurrentChanged)
    // Derived regen availability view from ecu-service.
    Q_PROPERTY(bool regenAvailable READ regenAvailable NOTIFY regenAvailableChanged)
    Q_PROPERTY(QString regenReason READ regenReason NOTIFY regenReasonChanged)
    Q_PROPERTY(double regenExpected READ regenExpected NOTIFY regenExpectedChanged)
    Q_PROPERTY(double rpm READ rpm NOTIFY rpmChanged)
    Q_PROPERTY(double speed READ speed NOTIFY speedChanged)
    Q_PROPERTY(double rawSpeed READ rawSpeed NOTIFY rawSpeedChanged)
    Q_PROPERTY(bool hasRawSpeed READ hasRawSpeed NOTIFY rawSpeedChanged)
    // throttle exposed as a bool (true = engaged) so QML can use it
    // naturally; the underlying Toggle enum has Bosch-style ordering
    // {On=0, Off=1}, which the cast to int would otherwise leak through.
    Q_PROPERTY(bool throttle READ throttle NOTIFY throttleChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY firmwareVersionChanged)
    Q_PROPERTY(double odometer READ odometer NOTIFY odometerChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(int faultCode READ faultCode NOTIFY faultCodeChanged)
    Q_PROPERTY(QString faultDescription READ faultDescription NOTIFY faultDescriptionChanged)
    Q_PROPERTY(QList<int> faults READ faults NOTIFY faultsChanged)

public:
    explicit EngineStore(MdbRepository *repo, QObject *parent = nullptr);

    int kers() const { return static_cast<int>(m_kers); }
    QString kersReasonOff() const { return m_kersReasonOff; }
    double motorVoltage() const { return m_motorVoltage; }
    double motorCurrent() const { return m_motorCurrent; }
    double acceptedRegenVoltage() const { return m_acceptedRegenVoltage; }
    double acceptedRegenCurrent() const { return m_acceptedRegenCurrent; }
    bool regenAvailable() const { return m_regenAvailable; }
    QString regenReason() const { return m_regenReason; }
    double regenExpected() const { return m_regenExpected; }
    double rpm() const { return m_rpm; }
    double speed() const { return m_speed; }
    double rawSpeed() const { return m_rawSpeed; }
    bool hasRawSpeed() const { return m_hasRawSpeed; }
    bool throttle() const { return m_throttle == ScootEnums::Toggle::On; }
    QString firmwareVersion() const { return m_firmwareVersion; }
    double odometer() const { return m_odometer; }
    double temperature() const { return m_temperature; }
    int faultCode() const { return m_faultCode; }
    QString faultDescription() const { return m_faultDescription; }
    QList<int> faults() const { return m_faults.values(); }

signals:
    void kersChanged();
    void kersReasonOffChanged();
    void motorVoltageChanged();
    void motorCurrentChanged();
    void acceptedRegenVoltageChanged();
    void acceptedRegenCurrentChanged();
    void regenAvailableChanged();
    void regenReasonChanged();
    void regenExpectedChanged();
    void rpmChanged();
    void speedChanged();
    void rawSpeedChanged();
    void throttleChanged();
    void firmwareVersionChanged();
    void odometerChanged();
    void temperatureChanged();
    void faultCodeChanged();
    void faultDescriptionChanged();
    void faultsChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;
    void applySetUpdate(const QString &name, const QStringList &members) override;

private:
    ScootEnums::Toggle m_kers = ScootEnums::Toggle::On;
    QString m_kersReasonOff;
    double m_motorVoltage = 0;
    double m_motorCurrent = 0;
    double m_acceptedRegenVoltage = 0;
    double m_acceptedRegenCurrent = 0;
    bool m_regenAvailable = true;
    QString m_regenReason = QStringLiteral("none");
    double m_regenExpected = 0;
    double m_rpm = 0;
    double m_speed = 0;
    double m_rawSpeed = 0;
    bool m_hasRawSpeed = false;
    ScootEnums::Toggle m_throttle = ScootEnums::Toggle::Off;
    QString m_firmwareVersion;
    double m_odometer = 0;
    double m_temperature = 0;
    int m_faultCode = 0;
    QString m_faultDescription;
    QSet<int> m_faults;
};

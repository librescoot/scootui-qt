#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"

class CbBatteryStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(int charge READ charge NOTIFY chargeChanged)
    Q_PROPERTY(int current READ current NOTIFY currentChanged)
    Q_PROPERTY(int remainingCapacity READ remainingCapacity NOTIFY remainingCapacityChanged)
    Q_PROPERTY(int temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(int cycleCount READ cycleCount NOTIFY cycleCountChanged)
    Q_PROPERTY(int timeToFull READ timeToFull NOTIFY timeToFullChanged)
    Q_PROPERTY(int timeToEmpty READ timeToEmpty NOTIFY timeToEmptyChanged)
    Q_PROPERTY(int cellVoltage READ cellVoltage NOTIFY cellVoltageChanged)
    Q_PROPERTY(int fullCapacity READ fullCapacity NOTIFY fullCapacityChanged)
    Q_PROPERTY(int stateOfHealth READ stateOfHealth NOTIFY stateOfHealthChanged)
    Q_PROPERTY(bool present READ present NOTIFY presentChanged)
    Q_PROPERTY(int chargeStatus READ chargeStatus NOTIFY chargeStatusChanged)
    Q_PROPERTY(QString partNumber READ partNumber NOTIFY partNumberChanged)
    Q_PROPERTY(QString serialNumber READ serialNumber NOTIFY serialNumberChanged)
    Q_PROPERTY(QString uniqueId READ uniqueId NOTIFY uniqueIdChanged)

public:
    explicit CbBatteryStore(MdbRepository *repo, QObject *parent = nullptr);

    int charge() const { return m_charge; }
    int current() const { return m_current; }
    int remainingCapacity() const { return m_remainingCapacity; }
    int temperature() const { return m_temperature; }
    int cycleCount() const { return m_cycleCount; }
    int timeToFull() const { return m_timeToFull; }
    int timeToEmpty() const { return m_timeToEmpty; }
    int cellVoltage() const { return m_cellVoltage; }
    int fullCapacity() const { return m_fullCapacity; }
    int stateOfHealth() const { return m_stateOfHealth; }
    bool present() const { return m_present; }
    int chargeStatus() const { return static_cast<int>(m_chargeStatus); }
    QString partNumber() const { return m_partNumber; }
    QString serialNumber() const { return m_serialNumber; }
    QString uniqueId() const { return m_uniqueId; }

signals:
    void chargeChanged();
    void currentChanged();
    void remainingCapacityChanged();
    void temperatureChanged();
    void cycleCountChanged();
    void timeToFullChanged();
    void timeToEmptyChanged();
    void cellVoltageChanged();
    void fullCapacityChanged();
    void stateOfHealthChanged();
    void presentChanged();
    void chargeStatusChanged();
    void partNumberChanged();
    void serialNumberChanged();
    void uniqueIdChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    int m_charge = 100;
    int m_current = 0;
    int m_remainingCapacity = 0;
    int m_temperature = 0;
    int m_cycleCount = 0;
    int m_timeToFull = 0;
    int m_timeToEmpty = 0;
    int m_cellVoltage = 0;
    int m_fullCapacity = 0;
    int m_stateOfHealth = 0;
    bool m_present = false;
    ScootEnums::ChargeStatus m_chargeStatus = ScootEnums::ChargeStatus::NotCharging;
    QString m_partNumber;
    QString m_serialNumber;
    QString m_uniqueId;
};

#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"

#include <QSet>

class BatteryStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(bool present READ present NOTIFY presentChanged)
    Q_PROPERTY(int batteryState READ batteryState NOTIFY batteryStateChanged)
    Q_PROPERTY(int voltage READ voltage NOTIFY voltageChanged)
    Q_PROPERTY(int current READ current NOTIFY currentChanged)
    Q_PROPERTY(int charge READ charge NOTIFY chargeChanged)
    Q_PROPERTY(int temperature0 READ temperature0 NOTIFY temperature0Changed)
    Q_PROPERTY(int temperature1 READ temperature1 NOTIFY temperature1Changed)
    Q_PROPERTY(int temperature2 READ temperature2 NOTIFY temperature2Changed)
    Q_PROPERTY(int temperature3 READ temperature3 NOTIFY temperature3Changed)
    Q_PROPERTY(QString temperatureState READ temperatureState NOTIFY temperatureStateChanged)
    Q_PROPERTY(int cycleCount READ cycleCount NOTIFY cycleCountChanged)
    Q_PROPERTY(int stateOfHealth READ stateOfHealth NOTIFY stateOfHealthChanged)
    Q_PROPERTY(QString serialNumber READ serialNumber NOTIFY serialNumberChanged)
    Q_PROPERTY(QString manufacturingDate READ manufacturingDate NOTIFY manufacturingDateChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY firmwareVersionChanged)
    Q_PROPERTY(QList<int> faults READ faults NOTIFY faultsChanged)

public:
    explicit BatteryStore(MdbRepository *repo, const QString &id, QObject *parent = nullptr);

    bool present() const { return m_present; }
    int batteryState() const { return static_cast<int>(m_batteryState); }
    int voltage() const { return m_voltage; }
    int current() const { return m_current; }
    int charge() const { return m_charge; }
    int temperature0() const { return m_temperature0; }
    int temperature1() const { return m_temperature1; }
    int temperature2() const { return m_temperature2; }
    int temperature3() const { return m_temperature3; }
    QString temperatureState() const { return m_temperatureState; }
    int cycleCount() const { return m_cycleCount; }
    int stateOfHealth() const { return m_stateOfHealth; }
    QString serialNumber() const { return m_serialNumber; }
    QString manufacturingDate() const { return m_manufacturingDate; }
    QString firmwareVersion() const { return m_firmwareVersion; }
    QList<int> faults() const { return m_faults.values(); }
    QString batteryId() const { return m_id; }

signals:
    void presentChanged();
    void batteryStateChanged();
    void voltageChanged();
    void currentChanged();
    void chargeChanged();
    void temperature0Changed();
    void temperature1Changed();
    void temperature2Changed();
    void temperature3Changed();
    void temperatureStateChanged();
    void cycleCountChanged();
    void stateOfHealthChanged();
    void serialNumberChanged();
    void manufacturingDateChanged();
    void firmwareVersionChanged();
    void faultsChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;
    void applySetUpdate(const QString &name, const QStringList &members) override;
    QString discriminatorValue() const override { return m_id; }

private:
    QString m_id;
    bool m_present = false;
    ScootEnums::BatteryState m_batteryState = ScootEnums::BatteryState::Unknown;
    int m_voltage = 0;
    int m_current = 0;
    int m_charge = 0;
    int m_temperature0 = 0;
    int m_temperature1 = 0;
    int m_temperature2 = 0;
    int m_temperature3 = 0;
    QString m_temperatureState;
    int m_cycleCount = 0;
    int m_stateOfHealth = 0;
    QString m_serialNumber;
    QString m_manufacturingDate;
    QString m_firmwareVersion;
    QSet<int> m_faults;
};

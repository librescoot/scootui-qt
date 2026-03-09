#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"

class AuxBatteryStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(int dateStreamEnable READ dateStreamEnable NOTIFY dateStreamEnableChanged)
    Q_PROPERTY(int voltage READ voltage NOTIFY voltageChanged)
    Q_PROPERTY(int charge READ charge NOTIFY chargeChanged)
    Q_PROPERTY(int chargeStatus READ chargeStatus NOTIFY chargeStatusChanged)

public:
    explicit AuxBatteryStore(MdbRepository *repo, QObject *parent = nullptr);

    int dateStreamEnable() const { return m_dateStreamEnable; }
    int voltage() const { return m_voltage; }
    int charge() const { return m_charge; }
    int chargeStatus() const { return static_cast<int>(m_chargeStatus); }

signals:
    void dateStreamEnableChanged();
    void voltageChanged();
    void chargeChanged();
    void chargeStatusChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    int m_dateStreamEnable = 0;
    int m_voltage = 12500;
    int m_charge = 100;
    ScootEnums::AuxChargeStatus m_chargeStatus = ScootEnums::AuxChargeStatus::FloatCharge;
};

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class BatteryStore;
class CbBatteryStore;
class AuxBatteryStore;
class VehicleStore;
class ToastService;
class Translations;

// Warns the rider when a backup battery (CBB / AUX) is running low - but only at
// the two moments the scooter enters an at-risk resting state:
//   1. it just transitioned from ready-to-drive to parked, or
//   2. the last main battery was removed from both slots.
// Outside those two edges the warning is NOT raised, even if a pack reads low
// (e.g. on boot, or while slowly draining over a long park) - the rider is only
// nagged at the natural moments they can act on it. Once raised the toast
// persists until the pack is no longer low (recharged / replaced).
//
// This is deliberately distinct from the charging-system warnings in
// BatteryDisplay.qml, which drive status-bar icons continuously (those stay as
// passive low indicators and are not edge-gated).
class BackupBatteryMonitor : public QObject
{
    Q_OBJECT

public:
    explicit BackupBatteryMonitor(BatteryStore *battery0, BatteryStore *battery1,
                                   CbBatteryStore *cbBattery, AuxBatteryStore *auxBattery,
                                   VehicleStore *vehicle, ToastService *toast,
                                   Translations *translations, QObject *parent = nullptr);

private slots:
    void evaluate();

private:
    bool noMainBattery() const;
    bool cbLow() const;
    bool auxLow() const;
    // Raise the armed warning(s) once the debounce has elapsed.
    void raise();

    // CBB reuses the same SoC gate as the charging-system warning.
    static constexpr int CbChargeThreshold = 50;
    // AUX has no fuel gauge (its SoC is just a bucketed copy of this voltage),
    // so gate on voltage. 11700 mV is the soft "low" line on the lead-acid curve;
    // a healthy 12V pack rests around 12.5V. A LiFePO4 aux would shift this.
    // Matches the status-bar mirror (auxLowVoltageMv in BatteryDisplay.qml).
    static constexpr int AuxVoltageThreshold = 11700; // mV
    static constexpr int DebounceMs = 1500;

    static const QString CbToastId;
    static const QString AuxToastId;

    BatteryStore *m_battery0;
    BatteryStore *m_battery1;
    CbBatteryStore *m_cbBattery;
    AuxBatteryStore *m_auxBattery;
    VehicleStore *m_vehicle;
    ToastService *m_toast;
    Translations *m_translations;
    QTimer *m_debounceTimer;
    bool m_cbShowing = false;
    bool m_auxShowing = false;

    // Edge-detection baselines and arming flags. m_prevState / m_prevNoMain are
    // seeded in the constructor so the initial state never counts as an edge.
    int m_prevState = 0;
    bool m_prevNoMain = false;
    bool m_armPark = false;
    bool m_armRemoval = false;
};

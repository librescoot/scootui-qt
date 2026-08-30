#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "BatteryAlertPolicy.h"

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

    // Thresholds shared with the status-bar mirror via BatteryAlertPolicy.h,
    // so the toast and the icon can never disagree about what "low" means.
    static constexpr int CbChargeThreshold = BatteryAlertPolicy::kCbLowChargePercent;
    static constexpr int AuxVoltageThreshold = BatteryAlertPolicy::kAuxLowVoltageMv;
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

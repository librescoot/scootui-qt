#pragma once

#include <QObject>
#include <QTimer>

class BatteryStore;
class AuxBatteryStore;
class ToastService;
class Translations;

// "Alternator" warning, car-style: a main pack is active, so the 12V system is
// expected to be charging, but the AUX charger reports not-charging. The pack's
// level is irrelevant here - this is about the charging path, not state of
// charge.
//
// Unlike BackupBatteryMonitor (which only speaks at the two resting edges where
// the rider can act), this fires while riding and mirrors a dash telltale: one
// transient toast per off->on transition, plus a status-bar icon
// (BatteryDisplay.qml) and a telltale-panel light (TelltalePanel.qml).
//
// The AUX pack has no `present` field in redis (unlike CBB), so "present" means
// the nRF52 has reported anything about it (voltage or charge).
class AuxChargeMonitor : public QObject
{
    Q_OBJECT

public:
    explicit AuxChargeMonitor(BatteryStore *battery0, AuxBatteryStore *auxBattery,
                              ToastService *toast, Translations *translations,
                              QObject *parent = nullptr, int debounceMs = DebounceMs);

    // Ride out bus transients during a battery swap before announcing.
    static constexpr int DebounceMs = 3000;

private slots:
    void evaluate();

private:
    bool conditionMet() const;
    void announce();

    BatteryStore *m_battery0;
    AuxBatteryStore *m_auxBattery;
    ToastService *m_toast;
    Translations *m_translations;
    QTimer *m_debounceTimer;
    bool m_conditionMet = false;
    bool m_announced = false;
};

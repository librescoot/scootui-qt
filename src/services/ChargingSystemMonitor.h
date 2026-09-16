#pragma once

#include <QObject>
#include <QTimer>

class BatteryStore;
class CbBatteryStore;
class AuxBatteryStore;
class ToastService;
class Translations;

// Car-style "charging system" warnings, one per backup pack. A main pack being
// active means the 12V system should be charging; when a backup charger reports
// not-charging in that state, raise one transient toast per off->on transition
// and re-arm when the condition clears.
//
// AUX is level-independent (charging path only, no fuel gauge). CBB has a real
// fuel gauge, so it keeps the same SoC gate as its status-bar icon - a healthy
// CBB that simply is not being topped up should not nag.
//
// Distinct from BackupBatteryMonitor, which only speaks at the two resting
// edges (park / last-battery-removed) and only when no main battery is present.
//
// The AUX pack has no `present` field in redis (unlike CBB), so "present" means
// the nRF52 has reported anything about it (voltage or charge).
class ChargingSystemMonitor : public QObject
{
    Q_OBJECT

public:
    explicit ChargingSystemMonitor(BatteryStore *battery0, CbBatteryStore *cbBattery,
                                   AuxBatteryStore *auxBattery, ToastService *toast,
                                   Translations *translations, QObject *parent = nullptr,
                                   int debounceMs = DebounceMs);

    // Ride out bus transients during a battery swap before announcing.
    static constexpr int DebounceMs = 3000;
    // Matches cbWarningCondition in BatteryDisplay.qml.
    static constexpr int CbChargeThreshold = 50;

private slots:
    void evaluateAux();
    void evaluateCb();

private:
    bool mainActive() const;
    bool auxConditionMet() const;
    bool cbConditionMet() const;

    // Independent latch per pack: recovering one must not swallow the other.
    struct Channel {
        QTimer *timer = nullptr;
        bool conditionMet = false;
        bool announced = false;
    };
    static void evaluateChannel(Channel &channel, bool conditionMet);

    BatteryStore *m_battery0;
    CbBatteryStore *m_cbBattery;
    AuxBatteryStore *m_auxBattery;
    ToastService *m_toast;
    Translations *m_translations;

    Channel m_aux;
    Channel m_cb;
};

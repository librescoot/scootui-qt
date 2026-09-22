#pragma once

#include <QObject>
#include <QTimer>

class BatteryStore;
class CbBatteryStore;
class AuxBatteryStore;
class SettingsStore;
class ToastService;
class Translations;

// Car-style "charging system" warnings, one per supplementary battery. A main
// pack being active means the 12V system should be charging; when a backup
// charger reports not-charging in that state, raise one transient toast per
// off->on transition and re-arm when the condition clears.
//
// AUX is gated on voltage (no fuel gauge); CBB keeps the SoC gate of its
// status-bar icon. Either can be silenced with its charging-system-warning
// setting.
//
// Distinct from BackupBatteryMonitor, which only speaks at the two resting
// edges (park / last-battery-removed) and only when no main battery is present.
//
// AUX has no `present` field in redis (unlike CBB), so "present" means the
// nRF52 has reported anything about it (voltage or charge).
class ChargingSystemMonitor : public QObject
{
    Q_OBJECT

public:
    explicit ChargingSystemMonitor(BatteryStore *battery0, CbBatteryStore *cbBattery,
                                   AuxBatteryStore *auxBattery, SettingsStore *settings,
                                   ToastService *toast, Translations *translations,
                                   QObject *parent = nullptr, int debounceMs = DebounceMs);

    // Ride out bus transients during a battery swap before announcing.
    static constexpr int DebounceMs = 3000;
    // Matches cbWarningCondition in BatteryDisplay.qml.
    static constexpr int CbChargeThreshold = 50;
    // At or above this, not-charging means the battery is full. Mirrors the QML
    // constants.
    static constexpr int AuxChargeCeilingMv = 14500;

private slots:
    void evaluateAux();
    void evaluateCb();

private:
    bool mainActive() const;
    bool auxConditionMet() const;
    bool cbConditionMet() const;

    // Independent latch per battery: recovering one must not swallow the other.
    struct Channel {
        QTimer *timer = nullptr;
        bool conditionMet = false;
        bool announced = false;
    };
    static void evaluateChannel(Channel &channel, bool conditionMet);

    BatteryStore *m_battery0;
    CbBatteryStore *m_cbBattery;
    AuxBatteryStore *m_auxBattery;
    SettingsStore *m_settings;
    ToastService *m_toast;
    Translations *m_translations;

    Channel m_aux;
    Channel m_cb;
};

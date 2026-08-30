#pragma once

#include <QObject>

class BatteryStore;
class EngineStore;
class SettingsStore;
class ToastService;
class Translations;

// Toasts battery and ECU faults as they appear. Battery slot 1 is
// suppressed unless dual-battery mode is enabled, since a stock scooter's
// rear slot is not wired and any pack there reports nonsense.
class FaultNotifier : public QObject
{
    Q_OBJECT

public:
    FaultNotifier(BatteryStore *battery0, BatteryStore *battery1,
                  EngineStore *engine, SettingsStore *settings,
                  ToastService *toasts, Translations *translations,
                  QObject *parent = nullptr);

private:
    void onBatteryFaults(BatteryStore *battery);
    void onEcuFault();

    EngineStore *m_engine;
    SettingsStore *m_settings;
    ToastService *m_toasts;
    Translations *m_translations;
};

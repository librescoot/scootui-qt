#pragma once

#include <QObject>
#include <QTimer>

class EngineStore;
class BatteryStore;
class CbBatteryStore;
class ToastService;

class LowTemperatureMonitor : public QObject
{
    Q_OBJECT

public:
    explicit LowTemperatureMonitor(EngineStore *engine, BatteryStore *battery0,
                                    CbBatteryStore *cbBattery, ToastService *toast,
                                    QObject *parent = nullptr);

private slots:
    void checkTemperatures();

private:
    bool isLowTemperature() const;

    static constexpr int EngineThreshold = 4;
    static constexpr int BatteryThreshold = 4;
    static constexpr int CbBatteryThreshold = 10;
    static constexpr int DebounceMs = 2000;

    EngineStore *m_engine;
    BatteryStore *m_battery0;
    CbBatteryStore *m_cbBattery;
    ToastService *m_toast;
    QTimer *m_debounceTimer;
    bool m_hasShownWarning = false;
    bool m_conditionMet = false;
};

#include "LowTemperatureMonitor.h"
#include "ToastService.h"
#include "stores/EngineStore.h"
#include "stores/BatteryStore.h"
#include "stores/CbBatteryStore.h"

LowTemperatureMonitor::LowTemperatureMonitor(EngineStore *engine, BatteryStore *battery0,
                                               CbBatteryStore *cbBattery, ToastService *toast,
                                               QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_battery0(battery0)
    , m_cbBattery(cbBattery)
    , m_toast(toast)
    , m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(DebounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        if (m_conditionMet && !m_hasShownWarning) {
            m_hasShownWarning = true;
            m_toast->showWarning(tr("Low temperature detected. Reduced performance possible."));
        }
    });

    auto trigger = [this]() { checkTemperatures(); };
    connect(m_engine, &EngineStore::temperatureChanged, this, trigger);
    connect(m_battery0, &BatteryStore::temperature2Changed, this, trigger);
    connect(m_battery0, &BatteryStore::temperature3Changed, this, trigger);
    connect(m_cbBattery, &CbBatteryStore::temperatureChanged, this, trigger);
}

void LowTemperatureMonitor::checkTemperatures()
{
    if (m_hasShownWarning) return;

    bool low = isLowTemperature();
    if (low && !m_conditionMet) {
        m_conditionMet = true;
        m_debounceTimer->start();
    } else if (!low) {
        m_conditionMet = false;
        m_debounceTimer->stop();
    }
}

bool LowTemperatureMonitor::isLowTemperature() const
{
    // Engine temperature (must be non-zero to be valid)
    int engineTemp = static_cast<int>(m_engine->temperature());
    if (engineTemp != 0 && engineTemp <= EngineThreshold)
        return true;

    // Battery 0 temp2 or temp3 (must be non-zero)
    int batt0Temp2 = m_battery0->temperature2();
    int batt0Temp3 = m_battery0->temperature3();
    if ((batt0Temp2 != 0 && batt0Temp2 <= BatteryThreshold) ||
        (batt0Temp3 != 0 && batt0Temp3 <= BatteryThreshold))
        return true;

    // CB battery (must be non-zero)
    int cbTemp = m_cbBattery->temperature();
    if (cbTemp != 0 && cbTemp <= CbBatteryThreshold)
        return true;

    return false;
}

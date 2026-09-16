#include "ChargingSystemMonitor.h"
#include "ToastService.h"
#include "stores/BatteryStore.h"
#include "stores/CbBatteryStore.h"
#include "stores/AuxBatteryStore.h"
#include "models/Enums.h"
#include "l10n/Translations.h"

void ChargingSystemMonitor::evaluateChannel(Channel &channel, bool conditionMet)
{
    if (!conditionMet) {
        // Cleared (charger recovered, main went inactive, ...). Re-arm so a
        // later failure announces again.
        channel.conditionMet = false;
        channel.announced = false;
        channel.timer->stop();
        return;
    }
    if (!channel.conditionMet) {
        channel.conditionMet = true;
        channel.timer->start();
    }
}

ChargingSystemMonitor::ChargingSystemMonitor(BatteryStore *battery0, CbBatteryStore *cbBattery,
                                             AuxBatteryStore *auxBattery, ToastService *toast,
                                             Translations *translations, QObject *parent,
                                             int debounceMs)
    : QObject(parent)
    , m_battery0(battery0)
    , m_cbBattery(cbBattery)
    , m_auxBattery(auxBattery)
    , m_toast(toast)
    , m_translations(translations)
{
    auto makeTimer = [this, debounceMs]() {
        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(debounceMs);
        return timer;
    };
    m_aux.timer = makeTimer();
    m_cb.timer = makeTimer();

    connect(m_aux.timer, &QTimer::timeout, this, [this]() {
        if (m_aux.conditionMet && !m_aux.announced) {
            m_aux.announced = true;
            m_toast->showWarning(m_translations->batteryAuxNotCharging());
        }
    });
    connect(m_cb.timer, &QTimer::timeout, this, [this]() {
        if (m_cb.conditionMet && !m_cb.announced) {
            m_cb.announced = true;
            m_toast->showWarning(m_translations->batteryCbNotCharging());
        }
    });

    connect(m_battery0, &BatteryStore::presentChanged, this, &ChargingSystemMonitor::evaluateAux);
    connect(m_battery0, &BatteryStore::chargeChanged, this, &ChargingSystemMonitor::evaluateAux);
    connect(m_battery0, &BatteryStore::batteryStateChanged, this, &ChargingSystemMonitor::evaluateAux);
    connect(m_auxBattery, &AuxBatteryStore::voltageValidChanged, this, &ChargingSystemMonitor::evaluateAux);
    connect(m_auxBattery, &AuxBatteryStore::chargeValidChanged, this, &ChargingSystemMonitor::evaluateAux);
    connect(m_auxBattery, &AuxBatteryStore::chargeStatusChanged, this, &ChargingSystemMonitor::evaluateAux);

    connect(m_battery0, &BatteryStore::presentChanged, this, &ChargingSystemMonitor::evaluateCb);
    connect(m_battery0, &BatteryStore::chargeChanged, this, &ChargingSystemMonitor::evaluateCb);
    connect(m_battery0, &BatteryStore::batteryStateChanged, this, &ChargingSystemMonitor::evaluateCb);
    connect(m_cbBattery, &CbBatteryStore::presentChanged, this, &ChargingSystemMonitor::evaluateCb);
    connect(m_cbBattery, &CbBatteryStore::chargeChanged, this, &ChargingSystemMonitor::evaluateCb);
    connect(m_cbBattery, &CbBatteryStore::chargeValidChanged, this, &ChargingSystemMonitor::evaluateCb);
    connect(m_cbBattery, &CbBatteryStore::chargeStatusChanged, this, &ChargingSystemMonitor::evaluateCb);

    // The stores may already hold the failing state when the monitor is wired up
    // (services are created after the stores start), so evaluate once.
    evaluateAux();
    evaluateCb();
}

bool ChargingSystemMonitor::mainActive() const
{
    return m_battery0->present()
        && m_battery0->charge() > 0
        && m_battery0->batteryState() == static_cast<int>(ScootEnums::BatteryState::Active);
}

bool ChargingSystemMonitor::auxConditionMet() const
{
    const bool auxPresent = m_auxBattery->voltageValid() || m_auxBattery->chargeValid();
    const bool auxNotCharging = m_auxBattery->chargeStatus()
        == static_cast<int>(ScootEnums::AuxChargeStatus::NotCharging);
    return mainActive() && auxPresent && auxNotCharging;
}

bool ChargingSystemMonitor::cbConditionMet() const
{
    return mainActive()
        && m_cbBattery->present()
        && m_cbBattery->chargeValid()
        && m_cbBattery->charge() < CbChargeThreshold
        && m_cbBattery->chargeStatus()
           != static_cast<int>(ScootEnums::ChargeStatus::Charging);
}

void ChargingSystemMonitor::evaluateAux()
{
    evaluateChannel(m_aux, auxConditionMet());
}

void ChargingSystemMonitor::evaluateCb()
{
    evaluateChannel(m_cb, cbConditionMet());
}

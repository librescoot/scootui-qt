#include "AuxChargeMonitor.h"
#include "ToastService.h"
#include "stores/BatteryStore.h"
#include "stores/AuxBatteryStore.h"
#include "models/Enums.h"
#include "l10n/Translations.h"

AuxChargeMonitor::AuxChargeMonitor(BatteryStore *battery0, AuxBatteryStore *auxBattery,
                                   ToastService *toast, Translations *translations,
                                   QObject *parent, int debounceMs)
    : QObject(parent)
    , m_battery0(battery0)
    , m_auxBattery(auxBattery)
    , m_toast(toast)
    , m_translations(translations)
    , m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(debounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, &AuxChargeMonitor::announce);

    connect(m_battery0, &BatteryStore::presentChanged, this, &AuxChargeMonitor::evaluate);
    connect(m_battery0, &BatteryStore::chargeChanged, this, &AuxChargeMonitor::evaluate);
    connect(m_battery0, &BatteryStore::batteryStateChanged, this, &AuxChargeMonitor::evaluate);
    connect(m_auxBattery, &AuxBatteryStore::voltageValidChanged, this, &AuxChargeMonitor::evaluate);
    connect(m_auxBattery, &AuxBatteryStore::chargeValidChanged, this, &AuxChargeMonitor::evaluate);
    connect(m_auxBattery, &AuxBatteryStore::chargeStatusChanged, this, &AuxChargeMonitor::evaluate);

    // The stores may already hold the failing state when the monitor is wired up
    // (services are created after the stores start), so evaluate once.
    evaluate();
}

bool AuxChargeMonitor::conditionMet() const
{
    const bool mainActive = m_battery0->present()
        && m_battery0->charge() > 0
        && m_battery0->batteryState() == static_cast<int>(ScootEnums::BatteryState::Active);
    const bool auxPresent = m_auxBattery->voltageValid() || m_auxBattery->chargeValid();
    const bool auxNotCharging = m_auxBattery->chargeStatus()
        == static_cast<int>(ScootEnums::AuxChargeStatus::NotCharging);
    return mainActive && auxPresent && auxNotCharging;
}

void AuxChargeMonitor::evaluate()
{
    if (!conditionMet()) {
        // Cleared (aux started charging, main went inactive, etc.). Re-arm so a
        // later failure announces again.
        m_conditionMet = false;
        m_announced = false;
        m_debounceTimer->stop();
        return;
    }

    if (!m_conditionMet) {
        m_conditionMet = true;
        m_debounceTimer->start();
    }
}

void AuxChargeMonitor::announce()
{
    if (!m_conditionMet || m_announced)
        return;
    m_announced = true;
    m_toast->showWarning(m_translations->batteryAuxNotCharging());
}

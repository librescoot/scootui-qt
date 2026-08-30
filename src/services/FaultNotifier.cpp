#include "FaultNotifier.h"

#include "l10n/Translations.h"
#include "services/ToastService.h"
#include "stores/BatteryStore.h"
#include "stores/EngineStore.h"
#include "stores/SettingsStore.h"
#include "utils/FaultFormatter.h"

FaultNotifier::FaultNotifier(BatteryStore *battery0, BatteryStore *battery1,
                             EngineStore *engine, SettingsStore *settings,
                             ToastService *toasts, Translations *translations,
                             QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_settings(settings)
    , m_toasts(toasts)
    , m_translations(translations)
{
    connect(battery0, &BatteryStore::faultsChanged, this,
            [this, battery0]() { onBatteryFaults(battery0); });
    connect(battery1, &BatteryStore::faultsChanged, this,
            [this, battery1]() { onBatteryFaults(battery1); });
    connect(m_engine, &EngineStore::faultCodeChanged, this,
            &FaultNotifier::onEcuFault);
}

void FaultNotifier::onBatteryFaults(BatteryStore *battery)
{
    auto faults = battery->faults();
    if (faults.isEmpty())
        return;
    // Suppress battery 1 fault toasts unless dual battery mode is enabled
    if (battery->batteryId() != QLatin1String("0") && !m_settings->dualBattery())
        return;
    QString slotName = battery->batteryId() == QLatin1String("0")
        ? m_translations->batterySlot0()
        : m_translations->batterySlot1();
    if (faults.size() == 1) {
        QString msg = slotName + QStringLiteral(": ")
            + FaultFormatter::formatSingleFault(faults.first(), m_translations);
        if (FaultFormatter::hasAnyCritical(faults))
            m_toasts->showError(msg);
        else
            m_toasts->showWarning(msg);
    } else {
        QString title = slotName + QStringLiteral(": ")
            + FaultFormatter::getMultipleFaultsTitle(faults, m_translations);
        QString detail = FaultFormatter::formatMultipleFaults(faults, m_translations);
        if (FaultFormatter::hasAnyCritical(faults))
            m_toasts->showError(title + QStringLiteral("\n") + detail);
        else
            m_toasts->showWarning(title + QStringLiteral("\n") + detail);
    }
}

// Mirrors the battery pattern but with "E" prefix and different
// code-to-description mapping. Single-code, no fault set.
void FaultNotifier::onEcuFault()
{
    int code = m_engine->faultCode();
    if (code == 0)
        return;
    QString msg = FaultFormatter::formatEcuFault(code, m_translations);
    if (FaultFormatter::getEcuSeverity(code) == FaultSeverity::Critical)
        m_toasts->showError(msg);
    else
        m_toasts->showWarning(msg);
}

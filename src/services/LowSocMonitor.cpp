#include "LowSocMonitor.h"
#include "ToastService.h"
#include "stores/BatteryStore.h"
#include "l10n/Translations.h"

LowSocMonitor::LowSocMonitor(BatteryStore *battery0, ToastService *toast,
                             Translations *translations, QObject *parent)
    : QObject(parent)
    , m_battery0(battery0)
    , m_toast(toast)
    , m_translations(translations)
{
    connect(m_battery0, &BatteryStore::chargeChanged, this, &LowSocMonitor::checkSoc);
    connect(m_battery0, &BatteryStore::presentChanged, this, &LowSocMonitor::reseed);

    // Seed the baseline so the first observed value never counts as a crossing.
    reseed();
}

void LowSocMonitor::reseed()
{
    m_lastSoc = m_battery0->charge();
    m_hasLastSoc = true;
}

void LowSocMonitor::checkSoc()
{
    const int soc = m_battery0->charge();

    // A removed pack reports stale/default data - re-baseline on (re)insertion
    // instead of comparing against the previous pack.
    if (!m_battery0->present()) {
        reseed();
        return;
    }
    if (!m_hasLastSoc || soc == m_lastSoc)
        return;

    if (soc < m_lastSoc) {
        const int previous = m_lastSoc;
        QString message;
        if (soc == 0 && previous > 0) {
            message = m_translations->batteryEmptyRecharge();
        } else if (soc < 5 && previous >= 5) {
            message = m_translations->batteryMaxSpeedReduced();
        } else if (soc <= 10 && previous > 10) {
            message = m_translations->batteryLowPowerReduced();
        } else if (soc < 20 && previous >= 20) {
            message = m_translations->batteryLowPowerReducedShort();
        }
        if (!message.isEmpty())
            m_toast->showWarning(message);
    }

    m_lastSoc = soc;
}

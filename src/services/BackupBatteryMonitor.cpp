#include "BackupBatteryMonitor.h"
#include "ToastService.h"
#include "stores/BatteryStore.h"
#include "stores/CbBatteryStore.h"
#include "stores/AuxBatteryStore.h"
#include "l10n/Translations.h"

const QString BackupBatteryMonitor::CbToastId = QStringLiteral("backup-cb-low");
const QString BackupBatteryMonitor::AuxToastId = QStringLiteral("backup-aux-low");

BackupBatteryMonitor::BackupBatteryMonitor(BatteryStore *battery0, BatteryStore *battery1,
                                             CbBatteryStore *cbBattery, AuxBatteryStore *auxBattery,
                                             ToastService *toast, Translations *translations,
                                             QObject *parent)
    : QObject(parent)
    , m_battery0(battery0)
    , m_battery1(battery1)
    , m_cbBattery(cbBattery)
    , m_auxBattery(auxBattery)
    , m_toast(toast)
    , m_translations(translations)
    , m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(DebounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        if (cbLowCondition() && !m_cbShowing) {
            m_cbShowing = true;
            m_toast->showPermanentWarning(m_translations->warningBackupCbLow(), CbToastId,
                                          QStringLiteral("qrc:/ScootUI/assets/icons/librescoot-cb-battery-blank.svg"));
        }
        if (auxLowCondition() && !m_auxShowing) {
            m_auxShowing = true;
            m_toast->showPermanentWarning(m_translations->warningBackupAuxLow(), AuxToastId,
                                          QStringLiteral("qrc:/ScootUI/assets/icons/librescoot-aux-battery-blank.svg"));
        }
    });

    connect(m_battery0, &BatteryStore::presentChanged, this, &BackupBatteryMonitor::evaluate);
    connect(m_battery1, &BatteryStore::presentChanged, this, &BackupBatteryMonitor::evaluate);
    connect(m_cbBattery, &CbBatteryStore::presentChanged, this, &BackupBatteryMonitor::evaluate);
    connect(m_cbBattery, &CbBatteryStore::chargeChanged, this, &BackupBatteryMonitor::evaluate);
    connect(m_cbBattery, &CbBatteryStore::chargeValidChanged, this, &BackupBatteryMonitor::evaluate);
    connect(m_auxBattery, &AuxBatteryStore::voltageChanged, this, &BackupBatteryMonitor::evaluate);
    connect(m_auxBattery, &AuxBatteryStore::voltageValidChanged, this, &BackupBatteryMonitor::evaluate);
}

bool BackupBatteryMonitor::noMainBattery() const
{
    return !m_battery0->present() && !m_battery1->present();
}

bool BackupBatteryMonitor::cbLowCondition() const
{
    // Only act on a reported SoC; "never received" is not a low reading.
    return noMainBattery()
        && m_cbBattery->present()
        && m_cbBattery->chargeValid()
        && m_cbBattery->charge() < CbChargeThreshold;
}

bool BackupBatteryMonitor::auxLowCondition() const
{
    if (!noMainBattery())
        return false;
    // Voltage-only: the AUX pack has no fuel gauge, its SoC is just a 5-bucket
    // quantization of this same voltage, so gate on the voltage directly. Only
    // act on a reported reading - "never received" is not a low reading.
    return m_auxBattery->voltageValid()
        && m_auxBattery->voltage() < AuxVoltageThreshold;
}

void BackupBatteryMonitor::evaluate()
{
    const bool cb = cbLowCondition();
    const bool aux = auxLowCondition();

    // Dismiss immediately when a condition clears (e.g. a main battery is inserted).
    if (!cb && m_cbShowing) {
        m_cbShowing = false;
        m_toast->dismiss(CbToastId);
    }
    if (!aux && m_auxShowing) {
        m_auxShowing = false;
        m_toast->dismiss(AuxToastId);
    }

    // Debounce before showing, to ride out transients during a battery swap.
    const bool needShow = (cb && !m_cbShowing) || (aux && !m_auxShowing);
    if (needShow) {
        if (!m_debounceTimer->isActive())
            m_debounceTimer->start();
    } else {
        m_debounceTimer->stop();
    }
}

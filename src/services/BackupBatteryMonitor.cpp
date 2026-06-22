#include "BackupBatteryMonitor.h"
#include "ToastService.h"
#include "stores/BatteryStore.h"
#include "stores/CbBatteryStore.h"
#include "stores/AuxBatteryStore.h"
#include "stores/VehicleStore.h"
#include "models/Enums.h"
#include "l10n/Translations.h"

const QString BackupBatteryMonitor::CbToastId = QStringLiteral("backup-cb-low");
const QString BackupBatteryMonitor::AuxToastId = QStringLiteral("backup-aux-low");

BackupBatteryMonitor::BackupBatteryMonitor(BatteryStore *battery0, BatteryStore *battery1,
                                             CbBatteryStore *cbBattery, AuxBatteryStore *auxBattery,
                                             VehicleStore *vehicle, ToastService *toast,
                                             Translations *translations, QObject *parent)
    : QObject(parent)
    , m_battery0(battery0)
    , m_battery1(battery1)
    , m_cbBattery(cbBattery)
    , m_auxBattery(auxBattery)
    , m_vehicle(vehicle)
    , m_toast(toast)
    , m_translations(translations)
    , m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(DebounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, &BackupBatteryMonitor::raise);

    // Seed edge-detection baselines so the initial state never counts as an edge
    // (e.g. booting with no main battery must not raise a "removed" warning).
    m_prevState = m_vehicle->state();
    m_prevNoMain = noMainBattery();

    connect(m_battery0, &BatteryStore::presentChanged, this, &BackupBatteryMonitor::evaluate);
    connect(m_battery1, &BatteryStore::presentChanged, this, &BackupBatteryMonitor::evaluate);
    connect(m_vehicle, &VehicleStore::stateChanged, this, &BackupBatteryMonitor::evaluate);
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

bool BackupBatteryMonitor::cbLow() const
{
    // Only act on a reported SoC; "never received" is not a low reading.
    return m_cbBattery->present()
        && m_cbBattery->chargeValid()
        && m_cbBattery->charge() < CbChargeThreshold;
}

bool BackupBatteryMonitor::auxLow() const
{
    // Voltage-only: the AUX pack has no fuel gauge, its SoC is just a 5-bucket
    // quantization of this same voltage, so gate on the voltage directly. Only
    // act on a reported reading - "never received" is not a low reading.
    return m_auxBattery->voltageValid()
        && m_auxBattery->voltage() < AuxVoltageThreshold;
}

void BackupBatteryMonitor::evaluate()
{
    const int state = m_vehicle->state();
    const bool noMain = noMainBattery();

    // --- Edge detection: arm the warning only at the two moments the rider
    //     enters an at-risk resting state. ---
    const bool parkEdge =
        m_prevState == static_cast<int>(ScootEnums::VehicleState::ReadyToDrive)
        && state == static_cast<int>(ScootEnums::VehicleState::Parked);
    const bool removalEdge = !m_prevNoMain && noMain;

    m_prevState = state;
    m_prevNoMain = noMain;

    if (parkEdge)
        m_armPark = true;
    if (removalEdge)
        m_armRemoval = true;

    // Dismiss immediately when a pack is no longer low (recharged / replaced).
    if (!cbLow() && m_cbShowing) {
        m_cbShowing = false;
        m_toast->dismiss(CbToastId);
    }
    if (!auxLow() && m_auxShowing) {
        m_auxShowing = false;
        m_toast->dismiss(AuxToastId);
    }

    // Debounce before raising, to ride out transients during a battery swap.
    if ((m_armPark || m_armRemoval) && !m_debounceTimer->isActive())
        m_debounceTimer->start();
}

void BackupBatteryMonitor::raise()
{
    // Re-check the arming guard now that the debounce has elapsed: a park is
    // still a park, but a "removed last battery" edge is void if a pack was
    // reinserted within the debounce window (the common battery-swap case).
    const bool parked = m_vehicle->state() == static_cast<int>(ScootEnums::VehicleState::Parked);
    const bool relevant = (m_armPark && parked) || (m_armRemoval && noMainBattery());
    m_armPark = false;
    m_armRemoval = false;
    if (!relevant)
        return;

    if (cbLow() && !m_cbShowing) {
        m_cbShowing = true;
        m_toast->showPermanentWarning(m_translations->warningBackupCbLow(), CbToastId,
                                      QStringLiteral("qrc:/ScootUI/assets/icons/librescoot-cb-battery-blank.svg"));
    }
    if (auxLow() && !m_auxShowing) {
        m_auxShowing = true;
        m_toast->showPermanentWarning(m_translations->warningBackupAuxLow(), AuxToastId,
                                      QStringLiteral("qrc:/ScootUI/assets/icons/librescoot-aux-battery-blank.svg"));
    }
}

#include "BatteryAlertModel.h"

#include "BatteryAlertPolicy.h"
#include "stores/BatteryStore.h"
#include "stores/CbBatteryStore.h"
#include "stores/AuxBatteryStore.h"
#include "stores/VehicleStore.h"
#include "models/Enums.h"

namespace {

BatteryAlertPolicy::Inputs snapshot(const BatteryStore *b0, const BatteryStore *b1,
                                    const CbBatteryStore *cb, const AuxBatteryStore *aux,
                                    const VehicleStore *vehicle)
{
    BatteryAlertPolicy::Inputs in;
    if (b0) {
        in.main0Present = b0->present();
        in.main0Charge = b0->charge();
        in.main0State = b0->batteryState();
    }
    if (b1)
        in.main1Present = b1->present();
    if (cb) {
        in.cbPresent = cb->present();
        in.cbChargeValid = cb->chargeValid();
        in.cbCharge = cb->charge();
        in.cbChargeStatus = cb->chargeStatus();
    }
    if (aux) {
        in.auxVoltageValid = aux->voltageValid();
        in.auxVoltageMv = aux->voltage();
        in.auxChargeStatus = aux->chargeStatus();
    }
    if (vehicle)
        in.seatboxClosed = vehicle->seatboxLock()
            == static_cast<int>(ScootEnums::SeatboxLock::Closed);
    return in;
}

}

BatteryAlertModel::BatteryAlertModel(BatteryStore *battery0, BatteryStore *battery1,
                                     CbBatteryStore *cbBattery, AuxBatteryStore *auxBattery,
                                     VehicleStore *vehicle, QObject *parent)
    : QObject(parent)
    , m_battery0(battery0)
    , m_battery1(battery1)
    , m_cbBattery(cbBattery)
    , m_auxBattery(auxBattery)
    , m_vehicle(vehicle)
{
    const auto arm = [this](Debounce &d, bool (*condition)(const BatteryAlertPolicy::Inputs &)) {
        d.timer.setSingleShot(true);
        d.timer.setInterval(BatteryAlertPolicy::kWarningDebounceMs);
        connect(&d.timer, &QTimer::timeout, this, [this, &d, condition]() {
            const auto in = snapshot(m_battery0, m_battery1, m_cbBattery, m_auxBattery, m_vehicle);
            if (condition(in) && !d.shown) {
                d.shown = true;
                emit alertsChanged();
            }
        });
    };
    arm(m_cbWarning, &BatteryAlertPolicy::cbWarning);
    arm(m_auxWarning, &BatteryAlertPolicy::auxWarning);
    arm(m_cbStranded, &BatteryAlertPolicy::cbStranded);
    arm(m_auxStranded, &BatteryAlertPolicy::auxStranded);

    if (m_battery0) {
        connect(m_battery0, &BatteryStore::presentChanged, this, &BatteryAlertModel::reevaluate);
        connect(m_battery0, &BatteryStore::chargeChanged, this, &BatteryAlertModel::reevaluate);
        connect(m_battery0, &BatteryStore::batteryStateChanged, this, &BatteryAlertModel::reevaluate);
    }
    if (m_battery1)
        connect(m_battery1, &BatteryStore::presentChanged, this, &BatteryAlertModel::reevaluate);
    if (m_cbBattery) {
        connect(m_cbBattery, &CbBatteryStore::presentChanged, this, &BatteryAlertModel::reevaluate);
        connect(m_cbBattery, &CbBatteryStore::chargeChanged, this, &BatteryAlertModel::reevaluate);
        connect(m_cbBattery, &CbBatteryStore::chargeValidChanged, this, &BatteryAlertModel::reevaluate);
        connect(m_cbBattery, &CbBatteryStore::chargeStatusChanged, this, &BatteryAlertModel::reevaluate);
    }
    if (m_auxBattery) {
        connect(m_auxBattery, &AuxBatteryStore::voltageChanged, this, &BatteryAlertModel::reevaluate);
        connect(m_auxBattery, &AuxBatteryStore::voltageValidChanged, this, &BatteryAlertModel::reevaluate);
        connect(m_auxBattery, &AuxBatteryStore::chargeStatusChanged, this, &BatteryAlertModel::reevaluate);
    }
    if (m_vehicle)
        connect(m_vehicle, &VehicleStore::seatboxLockChanged, this, &BatteryAlertModel::reevaluate);

    reevaluate();
}

bool BatteryAlertModel::cbLow() const
{
    return BatteryAlertPolicy::cbLow(
        snapshot(m_battery0, m_battery1, m_cbBattery, m_auxBattery, m_vehicle));
}

bool BatteryAlertModel::auxLow() const
{
    return BatteryAlertPolicy::auxLow(
        snapshot(m_battery0, m_battery1, m_cbBattery, m_auxBattery, m_vehicle));
}

QString BatteryAlertModel::valueText(int charge, double soh, bool asRange,
                                     bool withDecimals) const
{
    return BatteryAlertPolicy::valueText(charge, soh, asRange, withDecimals);
}

bool BatteryAlertModel::updateDebounce(Debounce &d, bool condition)
{
    if (condition) {
        if (!d.active) {
            d.active = true;
            d.timer.start();
        }
        return false;
    }
    d.active = false;
    d.timer.stop();
    if (d.shown) {
        d.shown = false;
        return true;
    }
    return false;
}

void BatteryAlertModel::reevaluate()
{
    const auto in = snapshot(m_battery0, m_battery1, m_cbBattery, m_auxBattery, m_vehicle);
    updateDebounce(m_cbWarning, BatteryAlertPolicy::cbWarning(in));
    updateDebounce(m_auxWarning, BatteryAlertPolicy::auxWarning(in));
    updateDebounce(m_cbStranded, BatteryAlertPolicy::cbStranded(in));
    updateDebounce(m_auxStranded, BatteryAlertPolicy::auxStranded(in));
    // cbLow/auxLow are derived straight from the inputs, so any input change
    // may move them; notify unconditionally like the old QML bindings did.
    emit alertsChanged();
}

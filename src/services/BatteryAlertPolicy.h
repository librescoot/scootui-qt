#pragma once

#include <QString>
#include <QtMath>

#include "models/Enums.h"

// Pure battery warning and range logic shared by the status bar
// (BatteryAlertModel) and BackupBatteryMonitor. No Qt object state; every
// function is a function of its inputs, so the whole file is testable
// without a running dashboard.
namespace BatteryAlertPolicy {

// AUX 12V thresholds, in mV. The AUX pack has no fuel gauge: mdb-nrf52
// quantizes this same voltage into 5 SoC buckets (0/25/50/75/100), so SoC
// carries strictly less information than the voltage it is derived from.
// Drive every aux low/warning/critical decision from voltage; SoC is kept
// only for the charge-level glyph. These three values are the one place to
// shift for a future "aux chemistry = LiFePO4" setting (the firmware SoC
// table is lead-acid-specific and can't be reused for LiFePO4's flat curve).
// Tiers: 11700 (soft "low": icon visibility + stranded mirror),
//        11495 ~ firmware empty line (charging-system warning),
//        11000 critical.
inline constexpr int kAuxLowVoltageMv = 11700;
inline constexpr int kAuxWarnVoltageMv = 11495;
inline constexpr int kAuxCriticalVoltageMv = 11000;

// CBB has a real fuel gauge; "low" is plain SoC.
inline constexpr int kCbLowChargePercent = 50;

// Warning icons appear only after their condition has held this long.
inline constexpr int kWarningDebounceMs = 3000;

// Everything the warning conditions read, as plain values.
struct Inputs {
    bool main0Present = false;
    int main0Charge = 0;
    int main0State = 0;      // ScootEnums::BatteryState
    bool main1Present = false;
    bool cbPresent = false;
    bool cbChargeValid = false;
    int cbCharge = 0;
    int cbChargeStatus = 0;  // ScootEnums::ChargeStatus
    bool auxVoltageValid = false;
    int auxVoltageMv = 0;
    int auxChargeStatus = 0; // ScootEnums::AuxChargeStatus
    bool seatboxClosed = false;
};

// "Low" reads that gate the optional level glyphs. Only act on a reported
// value - "never received" is not a low reading.
inline bool cbLow(const Inputs &in)
{
    return in.cbChargeValid && in.cbCharge < kCbLowChargePercent;
}

inline bool auxLow(const Inputs &in)
{
    return in.auxVoltageValid && in.auxVoltageMv < kAuxLowVoltageMv;
}

// CB charging-system warning: charge low, not charging, main battery present
// and active, seatbox closed (an open seatbox interrupts charging anyway).
inline bool cbWarning(const Inputs &in)
{
    return in.cbPresent
        && in.cbCharge < kCbLowChargePercent
        && in.cbChargeStatus != static_cast<int>(ScootEnums::ChargeStatus::Charging)
        && in.main0Present && in.main0Charge > 0
        && in.main0State == static_cast<int>(ScootEnums::BatteryState::Active)
        && in.seatboxClosed;
}

// AUX charging-system warning: below the firmware empty line and not
// charging, or below the critical line outright; both only while a main
// battery is present with the seatbox closed.
inline bool auxWarning(const Inputs &in)
{
    if (!in.auxVoltageValid || !in.main0Present || !in.seatboxClosed)
        return false;
    const bool lowNotCharging = in.auxVoltageMv < kAuxWarnVoltageMv
        && in.auxChargeStatus == static_cast<int>(ScootEnums::AuxChargeStatus::NotCharging);
    const bool critical = in.auxVoltageMv < kAuxCriticalVoltageMv;
    return lowNotCharging || critical;
}

// "Stranded" warnings: backup battery low while NO main battery is inserted.
// Distinct from the charging-system warnings above (which require a main
// battery present and active). These fire regardless of seatbox state.
inline bool noMainBattery(const Inputs &in)
{
    return !in.main0Present && !in.main1Present;
}

inline bool cbStranded(const Inputs &in)
{
    return noMainBattery(in) && in.cbPresent && cbLow(in);
}

inline bool auxStranded(const Inputs &in)
{
    return noMainBattery(in) && auxLow(in);
}

// Range model for the "range" battery display mode: a full, healthy pack is
// counted as 45 km, scaled by state of health and charge.
inline constexpr double kFullPackRangeKm = 45.0;

inline double rangeKm(double sohPercent, double chargePercent)
{
    return kFullPackRangeKm * (sohPercent / 100.0) * (chargePercent / 100.0);
}

// The status-bar value: plain charge, or the range with one decimal while it
// is small enough for the decimal to matter (and allowed to be shown).
inline QString valueText(int charge, double sohPercent, bool asRange, bool withDecimals)
{
    if (!asRange)
        return QString::number(charge);
    const double km = rangeKm(sohPercent, charge);
    if (km >= 10.0 || !withDecimals)
        return QString::number(qFloor(km));
    return QString::number(km, 'f', 1);
}

}

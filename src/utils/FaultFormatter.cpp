#include "FaultFormatter.h"
#include "l10n/Translations.h"

#include <algorithm>

FaultSeverity FaultFormatter::getSeverity(int code)
{
    switch (code) {
    case 5:  // B5 - Critical over-temp
    case 6:  // B6 - Short circuit
    case 14: // B14 - Over-current discharging
    case 32: // B32 - BMS not following
    case 34: // B34 - BMS comm error
    case 35: // B35 - NFC reader error
        return FaultSeverity::Critical;
    default:
        return FaultSeverity::Warning;
    }
}

QString FaultFormatter::getDescription(int code, Translations *tr)
{
    switch (code) {
    case 1:  return tr->faultSignalWireBroken();
    case 2:  return tr->faultOverTempCharging();
    case 3:  return tr->faultUnderTempCharging();
    case 4:  return tr->faultOverTempDischarging();
    case 5:  return tr->faultCriticalOverTemp();
    case 6:  return tr->faultShortCircuit();
    case 7:  return tr->faultReserved();
    case 8:  return tr->faultUnderTempDischarging();
    case 9:  return tr->faultMosfetOverTemp();
    case 10: return tr->faultReserved();
    case 11: return tr->faultCellOverVoltage();
    case 12: return tr->faultCellUnderVoltage();
    case 13: return tr->faultOverCurrentCharging();
    case 14: return tr->faultOverCurrentDischarging();
    case 15:
    case 16: return tr->faultReserved();
    case 32: return tr->faultBmsNotFollowing();
    case 33: return tr->faultBmsZeroData();
    case 34: return tr->faultBmsCommError();
    case 35: return tr->faultNfcReaderError();
    default: return tr->faultUnknown();
    }
}

QString FaultFormatter::formatSingleFault(int code, Translations *tr)
{
    return QStringLiteral("B%1: %2").arg(code).arg(getDescription(code, tr));
}

QString FaultFormatter::formatMultipleFaults(const QList<int> &codes, Translations *tr)
{
    Q_UNUSED(tr)

    if (codes.isEmpty())
        return QString();

    // Sort by severity (critical first), then by code number
    QList<int> sorted = codes;
    std::sort(sorted.begin(), sorted.end(), [](int a, int b) {
        bool aCritical = getSeverity(a) == FaultSeverity::Critical;
        bool bCritical = getSeverity(b) == FaultSeverity::Critical;
        if (aCritical != bCritical)
            return aCritical;
        return a < b;
    });

    QStringList parts;
    parts.reserve(sorted.size());
    for (int code : sorted)
        parts.append(QStringLiteral("B%1").arg(code));

    return parts.join(QStringLiteral(", "));
}

bool FaultFormatter::hasAnyCritical(const QList<int> &codes)
{
    return std::any_of(codes.begin(), codes.end(), [](int code) {
        return getSeverity(code) == FaultSeverity::Critical;
    });
}

QString FaultFormatter::getMultipleFaultsTitle(const QList<int> &codes, Translations *tr)
{
    if (hasAnyCritical(codes))
        return tr->faultMultipleCritical();
    return tr->faultMultipleBattery();
}

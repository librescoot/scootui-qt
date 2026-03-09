#include "AuxBatteryStore.h"

AuxBatteryStore::AuxBatteryStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings AuxBatteryStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("aux-battery"), 30000,
        {
            {QStringLiteral("dateStreamEnable"), QStringLiteral("date-stream-enable")},
            {QStringLiteral("voltage"), QStringLiteral("voltage")},
            {QStringLiteral("charge"), QStringLiteral("charge")},
            {QStringLiteral("chargeStatus"), QStringLiteral("charge-status")},
        },
        {}, {}
    };
}

void AuxBatteryStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("date-stream-enable")) {
        int v = value.toInt(); if (v != m_dateStreamEnable) { m_dateStreamEnable = v; emit dateStreamEnableChanged(); }
    } else if (variable == QLatin1String("voltage")) {
        int v = value.toInt(); if (v != m_voltage) { m_voltage = v; emit voltageChanged(); }
    } else if (variable == QLatin1String("charge")) {
        int v = value.toInt(); if (v != m_charge) { m_charge = v; emit chargeChanged(); }
    } else if (variable == QLatin1String("charge-status")) {
        auto v = ScootEnums::parseAuxChargeStatus(value);
        if (v != m_chargeStatus) { m_chargeStatus = v; emit chargeStatusChanged(); }
    }
}

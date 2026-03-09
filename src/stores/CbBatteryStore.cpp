#include "CbBatteryStore.h"

CbBatteryStore::CbBatteryStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings CbBatteryStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("cb-battery"), 30000,
        {
            {QStringLiteral("charge"), QStringLiteral("charge")},
            {QStringLiteral("current"), QStringLiteral("current")},
            {QStringLiteral("remainingCapacity"), QStringLiteral("remaining-capacity")},
            {QStringLiteral("temperature"), QStringLiteral("temperature")},
            {QStringLiteral("cycleCount"), QStringLiteral("cycle-count")},
            {QStringLiteral("timeToFull"), QStringLiteral("time-to-full")},
            {QStringLiteral("timeToEmpty"), QStringLiteral("time-to-empty")},
            {QStringLiteral("cellVoltage"), QStringLiteral("cell-voltage")},
            {QStringLiteral("fullCapacity"), QStringLiteral("full-capacity")},
            {QStringLiteral("stateOfHealth"), QStringLiteral("state-of-health")},
            {QStringLiteral("present"), QStringLiteral("present")},
            {QStringLiteral("chargeStatus"), QStringLiteral("charge-status")},
            {QStringLiteral("partNumber"), QStringLiteral("part-number")},
            {QStringLiteral("serialNumber"), QStringLiteral("serial-number")},
            {QStringLiteral("uniqueId"), QStringLiteral("unique-id")},
        },
        {}, {}
    };
}

void CbBatteryStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("charge")) {
        int v = value.toInt(); if (v != m_charge) { m_charge = v; emit chargeChanged(); }
    } else if (variable == QLatin1String("current")) {
        int v = value.toInt(); if (v != m_current) { m_current = v; emit currentChanged(); }
    } else if (variable == QLatin1String("remaining-capacity")) {
        int v = value.toInt(); if (v != m_remainingCapacity) { m_remainingCapacity = v; emit remainingCapacityChanged(); }
    } else if (variable == QLatin1String("temperature")) {
        int v = value.toInt(); if (v != m_temperature) { m_temperature = v; emit temperatureChanged(); }
    } else if (variable == QLatin1String("cycle-count")) {
        int v = value.toInt(); if (v != m_cycleCount) { m_cycleCount = v; emit cycleCountChanged(); }
    } else if (variable == QLatin1String("time-to-full")) {
        int v = value.toInt(); if (v != m_timeToFull) { m_timeToFull = v; emit timeToFullChanged(); }
    } else if (variable == QLatin1String("time-to-empty")) {
        int v = value.toInt(); if (v != m_timeToEmpty) { m_timeToEmpty = v; emit timeToEmptyChanged(); }
    } else if (variable == QLatin1String("cell-voltage")) {
        int v = value.toInt(); if (v != m_cellVoltage) { m_cellVoltage = v; emit cellVoltageChanged(); }
    } else if (variable == QLatin1String("full-capacity")) {
        int v = value.toInt(); if (v != m_fullCapacity) { m_fullCapacity = v; emit fullCapacityChanged(); }
    } else if (variable == QLatin1String("state-of-health")) {
        int v = value.toInt(); if (v != m_stateOfHealth) { m_stateOfHealth = v; emit stateOfHealthChanged(); }
    } else if (variable == QLatin1String("present")) {
        bool v = (value == QLatin1String("true") || value == QLatin1String("1"));
        if (v != m_present) { m_present = v; emit presentChanged(); }
    } else if (variable == QLatin1String("charge-status")) {
        auto v = ScootEnums::parseChargeStatus(value);
        if (v != m_chargeStatus) { m_chargeStatus = v; emit chargeStatusChanged(); }
    } else if (variable == QLatin1String("part-number")) {
        if (value != m_partNumber) { m_partNumber = value; emit partNumberChanged(); }
    } else if (variable == QLatin1String("serial-number")) {
        if (value != m_serialNumber) { m_serialNumber = value; emit serialNumberChanged(); }
    } else if (variable == QLatin1String("unique-id")) {
        if (value != m_uniqueId) { m_uniqueId = value; emit uniqueIdChanged(); }
    }
}

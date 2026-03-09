#include "BatteryStore.h"

BatteryStore::BatteryStore(MdbRepository *repo, const QString &id, QObject *parent)
    : SyncableStore(repo, parent)
    , m_id(id)
{
}

SyncSettings BatteryStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("battery:") + m_id,
        30000,
        {
            {QStringLiteral("present"), QStringLiteral("present")},
            {QStringLiteral("state"), QStringLiteral("state")},
            {QStringLiteral("voltage"), QStringLiteral("voltage")},
            {QStringLiteral("current"), QStringLiteral("current")},
            {QStringLiteral("charge"), QStringLiteral("charge")},
            {QStringLiteral("temperature0"), QStringLiteral("temperature:0")},
            {QStringLiteral("temperature1"), QStringLiteral("temperature:1")},
            {QStringLiteral("temperature2"), QStringLiteral("temperature:2")},
            {QStringLiteral("temperature3"), QStringLiteral("temperature:3")},
            {QStringLiteral("temperatureState"), QStringLiteral("temperature-state")},
            {QStringLiteral("cycleCount"), QStringLiteral("cycle-count")},
            {QStringLiteral("stateOfHealth"), QStringLiteral("state-of-health")},
            {QStringLiteral("serialNumber"), QStringLiteral("serial-number")},
            {QStringLiteral("manufacturingDate"), QStringLiteral("manufacturing-date")},
            {QStringLiteral("firmwareVersion"), QStringLiteral("fw-version")},
        },
        {
            {QStringLiteral("fault"), QStringLiteral("battery:$id:fault"), 0},
        },
        QStringLiteral("id")
    };
}

void BatteryStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("present")) {
        bool v = (value == QLatin1String("true") || value == QLatin1String("1"));
        if (v != m_present) { m_present = v; emit presentChanged(); }
    } else if (variable == QLatin1String("state")) {
        auto v = ScootEnums::parseBatteryState(value);
        if (v != m_batteryState) { m_batteryState = v; emit batteryStateChanged(); }
    } else if (variable == QLatin1String("voltage")) {
        int v = value.toInt();
        if (v != m_voltage) { m_voltage = v; emit voltageChanged(); }
    } else if (variable == QLatin1String("current")) {
        int v = value.toInt();
        if (v != m_current) { m_current = v; emit currentChanged(); }
    } else if (variable == QLatin1String("charge")) {
        int v = value.toInt();
        if (v != m_charge) { m_charge = v; emit chargeChanged(); }
    } else if (variable == QLatin1String("temperature:0")) {
        int v = value.toInt();
        if (v != m_temperature0) { m_temperature0 = v; emit temperature0Changed(); }
    } else if (variable == QLatin1String("temperature:1")) {
        int v = value.toInt();
        if (v != m_temperature1) { m_temperature1 = v; emit temperature1Changed(); }
    } else if (variable == QLatin1String("temperature:2")) {
        int v = value.toInt();
        if (v != m_temperature2) { m_temperature2 = v; emit temperature2Changed(); }
    } else if (variable == QLatin1String("temperature:3")) {
        int v = value.toInt();
        if (v != m_temperature3) { m_temperature3 = v; emit temperature3Changed(); }
    } else if (variable == QLatin1String("temperature-state")) {
        if (value != m_temperatureState) { m_temperatureState = value; emit temperatureStateChanged(); }
    } else if (variable == QLatin1String("cycle-count")) {
        int v = value.toInt();
        if (v != m_cycleCount) { m_cycleCount = v; emit cycleCountChanged(); }
    } else if (variable == QLatin1String("state-of-health")) {
        int v = value.toInt();
        if (v != m_stateOfHealth) { m_stateOfHealth = v; emit stateOfHealthChanged(); }
    } else if (variable == QLatin1String("serial-number")) {
        if (value != m_serialNumber) { m_serialNumber = value; emit serialNumberChanged(); }
    } else if (variable == QLatin1String("manufacturing-date")) {
        if (value != m_manufacturingDate) { m_manufacturingDate = value; emit manufacturingDateChanged(); }
    } else if (variable == QLatin1String("fw-version")) {
        if (value != m_firmwareVersion) { m_firmwareVersion = value; emit firmwareVersionChanged(); }
    }
}

void BatteryStore::applySetUpdate(const QString &name, const QStringList &members)
{
    if (name == QLatin1String("fault")) {
        QSet<int> newFaults;
        for (const auto &m : members) {
            newFaults.insert(m.toInt());
        }
        if (newFaults != m_faults) {
            m_faults = newFaults;
            emit faultsChanged();
        }
    }
}

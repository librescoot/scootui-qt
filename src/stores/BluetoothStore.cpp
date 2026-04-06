#include "BluetoothStore.h"

BluetoothStore::BluetoothStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    s_instance = this;
}

SyncSettings BluetoothStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("ble"), 5000,
        {
            {QStringLiteral("status"), QStringLiteral("status")},
            {QStringLiteral("macAddress"), QStringLiteral("mac-address")},
            {QStringLiteral("pinCode"), QStringLiteral("pin-code")},
            {QStringLiteral("serviceHealth"), QStringLiteral("service-health")},
            {QStringLiteral("serviceError"), QStringLiteral("service-error")},
            {QStringLiteral("lastUpdate"), QStringLiteral("last-update")},
        },
        {}, {}
    };
}

void BluetoothStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("status")) {
        auto v = ScootEnums::parseConnectionStatus(value);
        if (v != m_status) { m_status = v; emit statusChanged(); }
    } else if (variable == QLatin1String("mac-address")) {
        if (value != m_macAddress) { m_macAddress = value; emit macAddressChanged(); }
    } else if (variable == QLatin1String("pin-code")) {
        if (value != m_pinCode) { m_pinCode = value; emit pinCodeChanged(); }
    } else if (variable == QLatin1String("service-health")) {
        if (value != m_serviceHealth) { m_serviceHealth = value; emit serviceHealthChanged(); }
    } else if (variable == QLatin1String("service-error")) {
        if (value != m_serviceError) { m_serviceError = value; emit serviceErrorChanged(); }
    } else if (variable == QLatin1String("last-update")) {
        if (value != m_lastUpdate) { m_lastUpdate = value; emit lastUpdateChanged(); }
    }
}

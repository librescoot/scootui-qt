#include "InternetStore.h"

InternetStore::InternetStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings InternetStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("internet"), 5000,
        {
            {QStringLiteral("modemState"), QStringLiteral("modem-state")},
            {QStringLiteral("unuCloud"), QStringLiteral("unu-cloud")},
            {QStringLiteral("status"), QStringLiteral("status")},
            {QStringLiteral("ipAddress"), QStringLiteral("ip-address")},
            {QStringLiteral("accessTech"), QStringLiteral("access-tech")},
            {QStringLiteral("signalQuality"), QStringLiteral("signal-quality")},
            {QStringLiteral("simImei"), QStringLiteral("sim-imei")},
            {QStringLiteral("simImsi"), QStringLiteral("sim-imsi")},
            {QStringLiteral("simIccid"), QStringLiteral("sim-iccid")},
        },
        {
            {QStringLiteral("fault"), QStringLiteral("internet:fault"), 0},
        },
        {}
    };
}

void InternetStore::applySetUpdate(const QString &name, const QStringList &members)
{
    if (name == QLatin1String("fault")) {
        QSet<int> newFaults;
        for (const auto &m : members)
            newFaults.insert(m.toInt());
        if (newFaults != m_faults) {
            m_faults = newFaults;
            emit faultsChanged();
        }
    }
}

void InternetStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("modem-state")) {
        auto v = ScootEnums::parseModemState(value);
        if (v != m_modemState) { m_modemState = v; emit modemStateChanged(); }
    } else if (variable == QLatin1String("unu-cloud")) {
        auto v = ScootEnums::parseConnectionStatus(value);
        if (v != m_unuCloud) { m_unuCloud = v; emit unuCloudChanged(); }
    } else if (variable == QLatin1String("status")) {
        auto v = ScootEnums::parseConnectionStatus(value);
        if (v != m_status) { m_status = v; emit statusChanged(); }
    } else if (variable == QLatin1String("ip-address")) {
        if (value != m_ipAddress) { m_ipAddress = value; emit ipAddressChanged(); }
    } else if (variable == QLatin1String("access-tech")) {
        if (value != m_accessTech) { m_accessTech = value; emit accessTechChanged(); }
    } else if (variable == QLatin1String("signal-quality")) {
        int v = value.toInt();
        if (v != m_signalQuality) { m_signalQuality = v; emit signalQualityChanged(); }
    } else if (variable == QLatin1String("sim-imei")) {
        if (value != m_simImei) { m_simImei = value; emit simImeiChanged(); }
    } else if (variable == QLatin1String("sim-imsi")) {
        if (value != m_simImsi) { m_simImsi = value; emit simImsiChanged(); }
    } else if (variable == QLatin1String("sim-iccid")) {
        if (value != m_simIccid) { m_simIccid = value; emit simIccidChanged(); }
    }
}

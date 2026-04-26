#include "ScooterStore.h"

ScooterStore::ScooterStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings ScooterStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("scooter"), 5000,
        {
            // Ambient temperature in °C, written by vehicle-service from the
            // dashboard temp sensor. Float (e.g. "8.7"). Empty = no reading yet.
            {QStringLiteral("temperature"), QStringLiteral("temperature"), /*clearable=*/true},
        },
        {}, {}
    };
}

void ScooterStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable != QLatin1String("temperature"))
        return;

    if (value.isEmpty()) {
        if (m_hasTemperature) {
            m_hasTemperature = false;
            m_temperature = 0.0;
            emit temperatureChanged();
        }
        return;
    }

    bool ok = false;
    const double v = value.toDouble(&ok);
    if (!ok)
        return;

    if (!m_hasTemperature || v != m_temperature) {
        m_temperature = v;
        m_hasTemperature = true;
        emit temperatureChanged();
    }
}

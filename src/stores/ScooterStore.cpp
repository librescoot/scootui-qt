#include "ScooterStore.h"

#include <cmath>

#include <QtMath>
#include "repositories/RedisSchema.h"

ScooterStore::ScooterStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings ScooterStore::syncSettings() const
{
    return SyncSettings{
        RedisSchema::hash::Scooter, 5000,
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
            m_smoothingClock.invalidate();
            emit temperatureChanged();
        }
        if (m_isFrostWarning) {
            m_isFrostWarning = false;
            emit frostChanged();
        }
        return;
    }

    bool ok = false;
    const double v = value.toDouble(&ok);
    if (!ok)
        return;

    const bool firstReading = !m_hasTemperature;
    const double prevDisplay = m_temperature;

    if (firstReading || !m_smoothingClock.isValid()) {
        // Seed the EMA from the first sample so we don't lag on startup.
        m_temperature = v;
        m_smoothingClock.start();
        m_hasTemperature = true;
    } else {
        const qint64 elapsedMs = m_smoothingClock.restart();
        const double dt = elapsedMs / 1000.0;
        const double alpha = 1.0 - std::exp(-dt / SmoothingTauSec);
        m_temperature += alpha * (v - m_temperature);
    }

    updateFrostState();

    // Only notify QML when the integer the binding renders actually moves,
    // so a sub-degree EMA crawl doesn't churn re-evaluation every poll.
    if (firstReading || qRound(prevDisplay) != qRound(m_temperature))
        emit temperatureChanged();
}

void ScooterStore::updateFrostState()
{
    const bool prevWarning = m_isFrostWarning;

    if (m_temperature < FrostWarningEnter)
        m_isFrostWarning = true;
    else if (m_temperature >= FrostWarningExit)
        m_isFrostWarning = false;

    if (m_isFrostWarning != prevWarning)
        emit frostChanged();
}

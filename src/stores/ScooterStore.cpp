#include "ScooterStore.h"

#include <cmath>

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
            m_rawTemperature = 0.0;
            m_temperature = 0.0;
            m_displayHeldSince.invalidate();
            emit temperatureChanged();
        }
        const bool frostChanged_ = m_isCold || m_isFrostWarning;
        m_isCold = false;
        m_isFrostWarning = false;
        if (frostChanged_)
            emit frostChanged();
        return;
    }

    bool ok = false;
    const double v = value.toDouble(&ok);
    if (!ok)
        return;

    m_rawTemperature = v;
    m_hasTemperature = true;
    updateFrostState();
    updateDisplayedTemperature();
}

void ScooterStore::updateFrostState()
{
    const bool prevCold = m_isCold;
    const bool prevWarning = m_isFrostWarning;

    if (m_rawTemperature < ColdEnter)
        m_isCold = true;
    else if (m_rawTemperature >= ColdExit)
        m_isCold = false;

    if (m_rawTemperature < FrostWarningEnter)
        m_isFrostWarning = true;
    else if (m_rawTemperature >= FrostWarningExit)
        m_isFrostWarning = false;

    if (m_isCold != prevCold || m_isFrostWarning != prevWarning)
        emit frostChanged();
}

void ScooterStore::updateDisplayedTemperature()
{
    const bool firstReading = !m_displayHeldSince.isValid();
    const bool inWarningZone = m_rawTemperature <= DisplayBypassBelow;
    const bool largeChange = std::fabs(m_rawTemperature - m_temperature) >= DisplayDeadband;
    const bool heldLongEnough = m_displayHeldSince.isValid()
                                && m_displayHeldSince.hasExpired(DisplayHoldMs);

    if (!(firstReading || inWarningZone || largeChange || heldLongEnough))
        return;

    if (m_temperature == m_rawTemperature && !firstReading) {
        // Reset hold window even when value didn't move, so the timeout
        // measures time-since-last-considered-fresh rather than time-since-
        // last-actual-change.
        m_displayHeldSince.restart();
        return;
    }

    m_temperature = m_rawTemperature;
    m_displayHeldSince.restart();
    emit temperatureChanged();
}

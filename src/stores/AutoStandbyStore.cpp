#include "AutoStandbyStore.h"

#include <QDateTime>

AutoStandbyStore::AutoStandbyStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    s_instance = this;
    m_tickTimer.setInterval(1000);
    m_tickTimer.setSingleShot(false);
    connect(&m_tickTimer, &QTimer::timeout, this, &AutoStandbyStore::recomputeRemaining);
}

SyncSettings AutoStandbyStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("vehicle"), 500,
        {
            // Unix timestamp (seconds) when auto-lock will fire; cleared when no
            // timer active. Published by vehicle-service via PublishAutoStandbyDeadline.
            {QStringLiteral("deadline"), QStringLiteral("auto-standby-deadline"), /*clearable=*/true},
        },
        {}, {}
    };
}

void AutoStandbyStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable != QLatin1String("auto-standby-deadline"))
        return;

    const qint64 newDeadline = value.isEmpty() ? 0 : value.toLongLong();
    if (newDeadline != m_deadline) {
        m_deadline = newDeadline;
        emit deadlineChanged();
    }

    recomputeRemaining();

    if (m_deadline > 0) {
        if (!m_tickTimer.isActive())
            m_tickTimer.start();
    } else {
        if (m_tickTimer.isActive())
            m_tickTimer.stop();
    }
}

void AutoStandbyStore::recomputeRemaining()
{
    int newRemaining = 0;
    if (m_deadline > 0) {
        const qint64 nowSec = QDateTime::currentSecsSinceEpoch();
        const qint64 diff = m_deadline - nowSec;
        newRemaining = diff > 0 ? static_cast<int>(diff) : 0;
    }

    if (newRemaining != m_remainingSeconds) {
        m_remainingSeconds = newRemaining;
        emit remainingSecondsChanged();
    }

    // Stop the tick when we've reached zero — no need to keep firing.
    if (m_remainingSeconds == 0 && m_tickTimer.isActive())
        m_tickTimer.stop();
}

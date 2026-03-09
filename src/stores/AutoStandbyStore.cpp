#include "AutoStandbyStore.h"

AutoStandbyStore::AutoStandbyStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings AutoStandbyStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("vehicle"), 500,
        {
            {QStringLiteral("autoStandbyRemaining"), QStringLiteral("auto-standby-remaining")},
        },
        {}, {}
    };
}

void AutoStandbyStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("auto-standby-remaining")) {
        int v = value.toInt();
        if (v != m_autoStandbyRemaining) { m_autoStandbyRemaining = v; emit autoStandbyRemainingChanged(); }
    }
}

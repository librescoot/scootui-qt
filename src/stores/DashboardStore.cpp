#include "DashboardStore.h"

DashboardStore::DashboardStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    s_instance = this;
}

SyncSettings DashboardStore::syncSettings() const
{
    return {
        QStringLiteral("dashboard"),
        500,
        {
            {QStringLiteral("debug"), QStringLiteral("debug")},
        },
        {},
        {}
    };
}

void DashboardStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("debug")) {
        QString mode = value.isEmpty() ? QStringLiteral("off") : value;
        if (mode != m_debugMode) {
            m_debugMode = mode;
            emit debugModeChanged();
        }
    }
}

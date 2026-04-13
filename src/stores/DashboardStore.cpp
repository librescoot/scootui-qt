#include "DashboardStore.h"

DashboardStore::DashboardStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
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

void DashboardStore::setBacklightEnabled(bool enabled)
{
    m_repo->set(QStringLiteral("dashboard"),
                QStringLiteral("backlight-enabled"),
                enabled ? QStringLiteral("true") : QStringLiteral("false"));
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

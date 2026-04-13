#pragma once

#include "SyncableStore.h"

class DashboardStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QString debugMode READ debugMode NOTIFY debugModeChanged)

public:
    explicit DashboardStore(MdbRepository *repo, QObject *parent = nullptr);

    QString debugMode() const { return m_debugMode; }

    Q_INVOKABLE void setBacklightEnabled(bool enabled);

signals:
    void debugModeChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_debugMode = QStringLiteral("off");
};

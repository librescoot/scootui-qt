#pragma once

#include "SyncableStore.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class DashboardStore : public SyncableStore
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(QString debugMode READ debugMode NOTIFY debugModeChanged)

public:
    explicit DashboardStore(MdbRepository *repo, QObject *parent = nullptr);
    static DashboardStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    QString debugMode() const { return m_debugMode; }

signals:
    void debugModeChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_debugMode = QStringLiteral("off");
    static inline DashboardStore *s_instance = nullptr;
};

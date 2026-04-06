#pragma once

#include "SyncableStore.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class AutoStandbyStore : public SyncableStore
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(int autoStandbyRemaining READ autoStandbyRemaining NOTIFY autoStandbyRemainingChanged)

public:
    explicit AutoStandbyStore(MdbRepository *repo, QObject *parent = nullptr);
    static AutoStandbyStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    int autoStandbyRemaining() const { return m_autoStandbyRemaining; }

signals:
    void autoStandbyRemainingChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    int m_autoStandbyRemaining = 0;
    static inline AutoStandbyStore *s_instance = nullptr;
};

#pragma once

#include "SyncableStore.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class AutoStandbyStore : public SyncableStore
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(qint64 deadline READ deadline NOTIFY deadlineChanged)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY remainingSecondsChanged)

public:
    explicit AutoStandbyStore(MdbRepository *repo, QObject *parent = nullptr);
    static AutoStandbyStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    qint64 deadline() const { return m_deadline; }
    int remainingSeconds() const { return m_remainingSeconds; }

signals:
    void deadlineChanged();
    void remainingSecondsChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    void recomputeRemaining();

    qint64 m_deadline = 0;       // Unix seconds; 0 = no timer active
    int m_remainingSeconds = 0;  // max(0, deadline - now); 0 when inactive
    QTimer m_tickTimer;          // 1 Hz tick while a deadline is active

    static inline AutoStandbyStore *s_instance = nullptr;
};

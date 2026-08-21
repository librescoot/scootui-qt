#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

#include "../repositories/MdbRepository.h"
#include <QtQml/qqmlengine.h>

class UmsLogStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QStringList logEntries READ logEntries NOTIFY logEntriesChanged)

public:
    explicit UmsLogStore(MdbRepository *repo, QObject *parent = nullptr);

    QStringList logEntries() const { return m_logEntries; }

    void startPolling();
    void stopPolling();
    void clear();

signals:
    void logEntriesChanged();

private:
    static QString stripTimestamp(const QString &entry);
    void poll();

    MdbRepository *m_repo;
    QTimer m_timer;
    QStringList m_logEntries;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static UmsLogStore *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(UmsLogStore *instance) { s_qmlInstance = instance; }

private:
    static inline UmsLogStore *s_qmlInstance = nullptr;
};

#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "../repositories/MdbRepository.h"

class QQmlEngine;
class QJSEngine;

class UmsLogStore : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(QStringList logEntries READ logEntries NOTIFY logEntriesChanged)

public:
    explicit UmsLogStore(MdbRepository *repo, QObject *parent = nullptr);
    static UmsLogStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

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
    static inline UmsLogStore *s_instance = nullptr;
};

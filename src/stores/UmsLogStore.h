#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

#include "../repositories/MdbRepository.h"

class UmsLogStore : public QObject
{
    Q_OBJECT
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
};

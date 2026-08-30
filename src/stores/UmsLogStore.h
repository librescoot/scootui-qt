#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

#include "../repositories/MdbRepository.h"

class UsbStore;

class UmsLogStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList logEntries READ logEntries NOTIFY logEntriesChanged)

public:
    explicit UmsLogStore(MdbRepository *repo, QObject *parent = nullptr);

    QStringList logEntries() const { return m_logEntries; }

    // Poll while ums-service is processing; clear once the stick is gone so
    // the next session starts with an empty log.
    void attachUsb(UsbStore *usb);

signals:
    void logEntriesChanged();

private:
    void startPolling();
    void stopPolling();
    void clear();
    static QString stripTimestamp(const QString &entry);
    void poll();

    MdbRepository *m_repo;
    QTimer m_timer;
    QStringList m_logEntries;
};

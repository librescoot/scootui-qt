#pragma once

#include <QObject>

class MdbRepository;

class ConnectionStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool prolongedDisconnect READ prolongedDisconnect NOTIFY prolongedDisconnectChanged)
    Q_PROPERTY(bool hasEverConnected READ hasEverConnected NOTIFY hasEverConnectedChanged)
    Q_PROPERTY(bool usingBackupConnection READ usingBackupConnection NOTIFY usingBackupConnectionChanged)

public:
    explicit ConnectionStore(MdbRepository *repo, QObject *parent = nullptr);

    bool prolongedDisconnect() const { return m_prolongedDisconnect; }
    bool hasEverConnected() const { return m_hasEverConnected; }
    bool usingBackupConnection() const { return m_usingBackupConnection; }

    // For simulator use
    Q_INVOKABLE void simulateUsbDisconnect(bool disconnected);

signals:
    void prolongedDisconnectChanged();
    void hasEverConnectedChanged();
    void usingBackupConnectionChanged();

private:
    MdbRepository *m_repo;
    bool m_prolongedDisconnect = false;
    bool m_hasEverConnected = false;
    bool m_usingBackupConnection = false;
};

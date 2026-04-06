#include "ConnectionStore.h"
#include "repositories/MdbRepository.h"
#include <QTimer>

ConnectionStore::ConnectionStore(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_hasEverConnected(repo->isConnected() || repo->isUsingBackupConnection())
    , m_usingBackupConnection(repo->isUsingBackupConnection())
{
    s_instance = this;
    connect(m_repo, &MdbRepository::connectionStateChanged, this, [this](bool connected) {
        if (connected && !m_hasEverConnected) {
            m_hasEverConnected = true;
            emit hasEverConnectedChanged();
        }
    });

    connect(m_repo, &MdbRepository::prolongedDisconnect, this, [this](bool disconnected) {
        if (disconnected != m_prolongedDisconnect) {
            m_prolongedDisconnect = disconnected;
            emit prolongedDisconnectChanged();
        }
    });

    connect(m_repo, &MdbRepository::usingBackupConnection, this, [this](bool usingBackup) {
        if (usingBackup != m_usingBackupConnection) {
            m_usingBackupConnection = usingBackup;
            emit usingBackupConnectionChanged();
        }
    });

    // If the repo already connected on backup before we were created,
    // defer signal emission so QML has time to bind before the toast fires
    if (m_usingBackupConnection) {
        QTimer::singleShot(0, this, [this]() {
            emit usingBackupConnectionChanged();
        });
    }
}

void ConnectionStore::simulateUsbDisconnect(bool disconnected)
{
    if (disconnected != m_usingBackupConnection) {
        m_usingBackupConnection = disconnected;
        emit usingBackupConnectionChanged();
    }
}

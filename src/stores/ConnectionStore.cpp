#include "ConnectionStore.h"
#include "repositories/MdbRepository.h"

ConnectionStore::ConnectionStore(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
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
}

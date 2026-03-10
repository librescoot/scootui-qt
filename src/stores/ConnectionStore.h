#pragma once

#include <QObject>

class MdbRepository;

class ConnectionStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool prolongedDisconnect READ prolongedDisconnect NOTIFY prolongedDisconnectChanged)
    Q_PROPERTY(bool hasEverConnected READ hasEverConnected NOTIFY hasEverConnectedChanged)

public:
    explicit ConnectionStore(MdbRepository *repo, QObject *parent = nullptr);

    bool prolongedDisconnect() const { return m_prolongedDisconnect; }
    bool hasEverConnected() const { return m_hasEverConnected; }

signals:
    void prolongedDisconnectChanged();
    void hasEverConnectedChanged();

private:
    MdbRepository *m_repo;
    bool m_prolongedDisconnect = false;
    bool m_hasEverConnected = false;
};

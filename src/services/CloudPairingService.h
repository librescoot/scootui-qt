#pragma once

#include "repositories/MdbRepository.h"
#include <QJsonObject>
#include <QTimer>

class CloudPairingService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(qint64 expiresAt READ expiresAt NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canStart READ canStart NOTIFY changed)
    Q_PROPERTY(bool canStop READ canStop NOTIFY changed)
    Q_PROPERTY(bool online READ online NOTIFY changed)
    Q_PROPERTY(bool expired READ expired NOTIFY changed)
    Q_PROPERTY(bool loadingStatus READ loadingStatus NOTIFY changed)
public:
    explicit CloudPairingService(MdbRepository *repo, QObject *parent = nullptr);
    ~CloudPairingService() override;
    QString state() const { return m_state; }
    QString code() const;
    qint64 expiresAt() const { return m_expiresAt; }
    bool running() const { return m_running; }
    bool busy() const { return m_busy && m_action != QLatin1String("status"); }
    bool canStart() const;
    bool canStop() const;
    bool online() const;
    bool expired() const { return m_expired; }
    bool loadingStatus() const { return m_busy && m_action == QLatin1String("status"); }
    void setInternetAvailable(bool available);
    void setVisible(bool visible);
    void setStationary(bool stationary);
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    void request(const QString &action);
    void receive(const QString &channel, const QString &data);
    void clearRequest();
    void unavailable();
    MdbRepository *m_repo;
    QTimer m_poll, m_timeout, m_expiry;
    QString m_state = QStringLiteral("unavailable"), m_code, m_channel, m_action;
    qint64 m_expiresAt = 0, m_deadline = 0;
    SubscriptionId m_subscription = 0;
    bool m_visible = false, m_stationary = false, m_running = false, m_busy = false, m_sent = false;
    bool m_internetAvailable = true, m_expired = false;
};

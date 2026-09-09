#pragma once

#include <QObject>
#include <QPointer>
#include "repositories/MdbRepository.h"

class NotificationService;

class NotificationIngress : public QObject
{
    Q_OBJECT
public:
    explicit NotificationIngress(MdbRepository *repository, NotificationService *notifications);
    ~NotificationIngress() override;

    bool receive(const QString &message);

signals:
    void rejected(const QString &reason);

private:
    bool reject(const QString &reason);
    QPointer<MdbRepository> m_repository;
    NotificationService *m_notifications;
    SubscriptionId m_subscription = 0;
};

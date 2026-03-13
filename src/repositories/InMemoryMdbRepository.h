#pragma once

#include "MdbRepository.h"

#include <QTimer>
#include <QHash>
#include <QSet>
#include <QRandomGenerator>

class InMemoryMdbRepository : public MdbRepository
{
    Q_OBJECT

public:
    explicit InMemoryMdbRepository(QObject *parent = nullptr);
    ~InMemoryMdbRepository() override;

    QString get(const QString &channel, const QString &variable) override;
    FieldMap getAll(const QString &channel) override;
    void set(const QString &channel, const QString &variable,
             const QString &value, bool publish = true) override;
    void publish(const QString &channel, const QString &message) override;
    void subscribe(const QString &channel, SubscriptionCallback callback) override;
    void unsubscribe(const QString &channel) override;
    void push(const QString &channel, const QString &command) override;
    void dashboardReady() override;
    void publishButtonEvent(const QString &event) override;

    QStringList getSetMembers(const QString &setKey) override;
    void addToSet(const QString &setKey, const QString &member) override;
    void removeFromSet(const QString &setKey, const QString &member) override;
    void hdel(const QString &key, const QString &field) override;
    QStringList lrange(const QString &key, int start, int stop) override;

private:
    void startBrightnessSimulation();
    void notifySubscribers(const QString &channel, const QString &variable);

    QHash<QString, QHash<QString, QString>> m_storage;
    QHash<QString, QSet<QString>> m_setStorage;
    QHash<QString, QList<SubscriptionCallback>> m_subscribers;
    QTimer *m_brightnessTimer = nullptr;
};

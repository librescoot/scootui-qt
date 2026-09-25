#pragma once

#include "MdbRepository.h"

#include <QTimer>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QRandomGenerator>

class InMemoryMdbRepository : public MdbRepository
{
    Q_OBJECT

public:
    explicit InMemoryMdbRepository(QObject *parent = nullptr);
    ~InMemoryMdbRepository() override;

    bool isConnected() const override { return true; }
    bool isDataSeeded() const override { return true; }

    QString get(const QString &channel, const QString &variable) override;
    FieldMap getAll(const QString &channel) override;
    void requestAll(const QString &channel) override;
    void requestValue(const QString &key) override;
    void setValue(const QString &key, const QString &value);
    void set(const QString &channel, const QString &variable,
             const QString &value, bool publish = true) override;
    void publish(const QString &channel, const QString &message) override;
    SubscriptionId subscribe(const QString &channel, SubscriptionCallback callback) override;
    void unsubscribe(const QString &channel, SubscriptionId id) override;
    void push(const QString &channel, const QString &command) override;
    void dashboardReady() override;
    void publishButtonEvent(const QString &event) override;

    QStringList getSetMembers(const QString &setKey) override;
    void addToSet(const QString &setKey, const QString &member) override;
    void removeFromSet(const QString &setKey, const QString &member) override;
    void hdel(const QString &key, const QString &field) override;
    QStringList lrange(const QString &key, int start, int stop) override;
    QVariantList xrevrange(const QString &key, int count) override;

private:
    void startBrightnessSimulation();
    void notifySubscribers(const QString &channel, const QString &variable);

    // In-process stand-in for settings-service's destination CallServer so
    // the dashboard's RPC path runs against this repository unchanged.
    void handleDestinationCall(const QString &envelopeJson);
    void handleRoutePlanCall(const QString &envelopeJson);
    void publishRoutePlan();
    bool destinationSave(const QJsonObject &payload, QVariantMap &result, QString &error);
    bool destinationDelete(const QJsonObject &payload, QString &error);
    bool destinationTouch(const QJsonObject &payload, QString &error);

    struct SubscriptionEntry {
        SubscriptionId id;
        SubscriptionCallback callback;
    };

    QHash<QString, QHash<QString, QString>> m_storage;
    QHash<QString, QString> m_valueStorage;
    QHash<QString, QSet<QString>> m_setStorage;
    QHash<QString, QList<SubscriptionEntry>> m_subscribers;
    SubscriptionId m_nextSubscriptionId = 1;
    QTimer *m_brightnessTimer = nullptr;
    QJsonObject m_routePlan{{QStringLiteral("id"), QString()},
                            {QStringLiteral("revision"), 0},
                            {QStringLiteral("stops"), QJsonArray{}},
                            {QStringLiteral("current_step"), 0},
                            {QStringLiteral("keep_current_stop"), false}};
};

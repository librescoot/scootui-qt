#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <functional>

using FieldMap = QHash<QString, QString>;
using SubscriptionCallback = std::function<void(const QString &channel, const QString &message)>;

class MdbRepository : public QObject
{
    Q_OBJECT

public:
    explicit MdbRepository(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~MdbRepository() = default;

    virtual QString get(const QString &channel, const QString &variable) = 0;
    virtual FieldMap getAll(const QString &channel) = 0;
    virtual void set(const QString &channel, const QString &variable,
                     const QString &value, bool publish = true) = 0;
    virtual void publish(const QString &channel, const QString &message) = 0;
    virtual void subscribe(const QString &channel, SubscriptionCallback callback) = 0;
    virtual void unsubscribe(const QString &channel) = 0;
    virtual void push(const QString &channel, const QString &command) = 0;
    virtual void dashboardReady() = 0;
    virtual void publishButtonEvent(const QString &event) = 0;

    // Set operations
    virtual QStringList getSetMembers(const QString &setKey) = 0;
    virtual void addToSet(const QString &setKey, const QString &member) = 0;
    virtual void removeFromSet(const QString &setKey, const QString &member) = 0;
    virtual void hdel(const QString &key, const QString &field) = 0;
    virtual QStringList lrange(const QString &key, int start, int stop) = 0;

    // Connection state queries (for stores that need initial state)
    virtual bool isConnected() const { return false; }
    virtual bool isUsingBackupConnection() const { return false; }

    // Re-emit current connection state so late listeners can sync
    void notifyConnectionState() { emit connectionStateChanged(isConnected()); }

signals:
    void connectionStateChanged(bool connected);
    void prolongedDisconnect(bool disconnected);
    void usingBackupConnection(bool usingBackup);
};

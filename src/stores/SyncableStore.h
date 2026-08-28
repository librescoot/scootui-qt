#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QHash>
#include <QSet>

#include "repositories/MdbRepository.h"

// Describes one field to sync from a Redis hash
struct SyncFieldDef {
    QString name;       // property name in the store
    QString variable;   // Redis hash field name (may differ from property name)
    bool clearable = false; // if true, field is cleared when missing from Redis
};

// Describes a set field to sync via SMEMBERS
struct SyncSetFieldDef {
    QString name;       // property name
    QString setKey;     // Redis set key (may contain $id for interpolation)
    int intervalMs = 0; // custom interval; 0 = use class interval
};

// Sync configuration for a store
struct SyncSettings {
    QString channel;
    int intervalMs;
    QList<SyncFieldDef> fields;
    QList<SyncSetFieldDef> setFields;
    QString discriminator; // e.g., "id" for batteries
};

class SyncableStore : public QObject
{
    Q_OBJECT

public:
    explicit SyncableStore(MdbRepository *repo, QObject *parent = nullptr);
    ~SyncableStore() override;

    virtual void start();
    virtual void stop();
    void refreshAllFields();

    // Records a value this process just wrote to the hash, without waiting for
    // the read-back. A store is the dashboard's picture of what Redis holds; if
    // it keeps the old value after a local write, then a service that rejects
    // or overrides that write puts back something the store already believes
    // and the update lands as a no-op with nothing to react to.
    void applyLocalWrite(const QString &variable, const QString &value);

protected:
    virtual SyncSettings syncSettings() const = 0;
    virtual void applyFieldUpdate(const QString &variable, const QString &value) = 0;
    virtual void applySetUpdate(const QString &name, const QStringList &members);
    // Hooks for stores that expose a coherent aggregate snapshot in addition
    // to individual properties. HGETALL fields are applied between these two
    // calls, allowing consumers to be notified only after the full hash is
    // internally consistent.
    virtual void beginBatchUpdate() {}
    virtual void endBatchUpdate() {}
    virtual QString discriminatorValue() const { return {}; }

    MdbRepository *m_repo;

private:
    void onFieldsReceived(const QString &channel, const FieldMap &fields);
    void onFieldFetched(const QString &channel, const QString &field, const QString &value);
    void onPubsubMessage(const QString &channel, const QString &message);
    void doRefreshSet(const SyncSetFieldDef &field);
    void scheduleSetTimer(const SyncSetFieldDef &field);
    void requestHashRefresh();
    QString interpolateKey(const QString &key) const;

    QHash<QString, QTimer*> m_setTimers;
    QString m_channel;
    SubscriptionId m_subscriptionId = 0;
    SyncSettings m_cachedSettings;
    bool m_started = false;

    // Coalescing window for pub/sub-triggered hash refreshes. One status
    // update from a service publishes every changed field separately, so
    // without this a single update costs one round trip per field.
    QTimer *m_refreshCooldown = nullptr;
    bool m_refreshPending = false;
};

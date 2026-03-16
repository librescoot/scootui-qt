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

    void start();
    void stop();
    void refreshAllFields();

protected:
    virtual SyncSettings syncSettings() const = 0;
    virtual void applyFieldUpdate(const QString &variable, const QString &value) = 0;
    virtual void applySetUpdate(const QString &name, const QStringList &members);
    virtual QString discriminatorValue() const { return {}; }

    MdbRepository *m_repo;

private:
    void doHgetall();
    void onPubsubMessage(const QString &channel, const QString &message);
    void startPollTimer();
    void doRefreshSet(const SyncSetFieldDef &field);
    void scheduleSetTimer(const SyncSetFieldDef &field);
    void pausePolling();
    void resumePolling();
    QString interpolateKey(const QString &key) const;

    QTimer *m_pollTimer = nullptr;
    QTimer *m_pubsubDebounce = nullptr;
    QHash<QString, QTimer*> m_setTimers;
    QString m_channel;  // cached from syncSettings() to avoid virtual call in destructor
    bool m_isPaused = false;
    bool m_isClosing = false;
    bool m_hasLoggedError = false;
    bool m_started = false;
};

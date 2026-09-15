#pragma once

#include "SyncableStore.h"

#include <QElapsedTimer>
#include <QTimer>

class EngineStore;
class VehicleStore;

class TripStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(double distance READ distance NOTIFY distanceChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double averageSpeed READ averageSpeed NOTIFY averageSpeedChanged)
    Q_PROPERTY(bool persistentAvailable READ persistentAvailable NOTIFY persistentAvailableChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion NOTIFY counterChanged)
    Q_PROPERTY(QString resetPolicy READ resetPolicy NOTIFY counterChanged)
    Q_PROPERTY(qint64 resetAt READ resetAt NOTIFY counterChanged)
    Q_PROPERTY(QString resetReason READ resetReason NOTIFY counterChanged)
    Q_PROPERTY(qint64 generation READ generation NOTIFY counterChanged)
    Q_PROPERTY(QString status READ status NOTIFY counterChanged)
    Q_PROPERTY(qint64 updatedAt READ updatedAt NOTIFY counterChanged)
    Q_PROPERTY(QString resetState READ resetState NOTIFY resetStateChanged)
    Q_PROPERTY(QString resetError READ resetError NOTIFY resetStateChanged)

public:
    explicit TripStore(MdbRepository *repo, EngineStore *engine, VehicleStore *vehicle,
                       QObject *parent = nullptr);
    ~TripStore() override;

    double distance() const { return m_overrideActive ? m_overrideDistance : m_distance; }
    int duration() const { return m_overrideActive ? m_overrideDuration : m_duration; }
    double averageSpeed() const { return m_overrideActive ? m_overrideAverageSpeed : m_averageSpeed; }
    bool persistentAvailable() const { return m_persistentAvailable; }
    QString apiVersion() const { return m_apiVersion; }
    QString resetPolicy() const { return m_resetPolicy; }
    qint64 resetAt() const { return m_resetAt; }
    QString resetReason() const { return m_resetReason; }
    qint64 generation() const { return m_generation; }
    QString status() const { return m_status; }
    qint64 updatedAt() const { return m_updatedAt; }
    QString resetState() const { return m_resetState; }
    QString resetError() const { return m_resetError; }

    void start() override;
    void stop() override;

    Q_INVOKABLE void reset();
    Q_INVOKABLE void setOverride(double distance_km, int duration_s, double avg_speed_kmh);
    Q_INVOKABLE void clearOverride();

signals:
    void distanceChanged();
    void durationChanged();
    void averageSpeedChanged();
    void persistentAvailableChanged();
    void counterChanged();
    void resetStateChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;
    void beginBatchUpdate() override;
    void endBatchUpdate() override;

private slots:
    void onVehicleStateChanged();
    void onTick();
    void onResetResultTimeout();
    void requestLiveness();

private:
    void startTracking();
    void pauseTracking();
    void setResetState(const QString &state, const QString &error = {});
    void handleCommandResult(const QString &message);
    void setPersistentAvailable(bool available);
    void updatePersistentAvailability();

    EngineStore *m_engine;
    VehicleStore *m_vehicle;
    QTimer *m_tickTimer = nullptr;
    QTimer m_resetResultTimer;
    QTimer m_livenessTimer;
    QElapsedTimer m_elapsed;
    SubscriptionId m_commandResultSubscription = 0;
    bool m_tracking = false;
    bool m_resetPending = true;
    qint64 m_accumulatedMs = 0;
    double m_distance = 0;
    int m_duration = 0;
    double m_averageSpeed = 0;
    bool m_overrideActive = false;
    double m_overrideDistance = 0;
    int m_overrideDuration = 0;
    double m_overrideAverageSpeed = 0;

    bool m_persistentAvailable = false;
    bool m_persistentSupported = false;
    bool m_serviceReady = false;
    bool m_apiVersionSeenInBatch = false;
    QString m_apiVersion;
    QString m_resetPolicy;
    qint64 m_resetAt = 0;
    QString m_resetReason;
    qint64 m_generation = 0;
    QString m_status;
    qint64 m_updatedAt = 0;
    QString m_resetState;
    QString m_resetError;
    QString m_pendingResetId;

    static constexpr int kResetResultTimeoutMs = 5000;
    static constexpr int kLivenessPollMs = 5000;
};

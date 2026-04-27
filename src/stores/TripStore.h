#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

class EngineStore;
class VehicleStore;

class TripStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double distance READ distance NOTIFY distanceChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double averageSpeed READ averageSpeed NOTIFY averageSpeedChanged)

public:
    explicit TripStore(EngineStore *engine, VehicleStore *vehicle, QObject *parent = nullptr);

    double distance() const { return m_distance; }
    int duration() const { return m_duration; }
    double averageSpeed() const { return m_averageSpeed; }

    Q_INVOKABLE void reset();
    Q_INVOKABLE void setOverride(double distance_km, int duration_s, double avg_speed_kmh);
    Q_INVOKABLE void clearOverride();

signals:
    void distanceChanged();
    void durationChanged();
    void averageSpeedChanged();

private slots:
    void onVehicleStateChanged();
    void onTick();

private:
    void startTracking();
    void pauseTracking();

    EngineStore *m_engine;
    VehicleStore *m_vehicle;
    QTimer *m_tickTimer = nullptr;
    QElapsedTimer m_elapsed;
    bool m_tracking = false;
    bool m_resetPending = true;
    qint64 m_accumulatedMs = 0;
    double m_distance = 0;
    int m_duration = 0;
    double m_averageSpeed = 0;
    bool m_overrideActive = false;
};

#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QtQml/qqmlregistration.h>

class EngineStore;
class VehicleStore;

class QQmlEngine;
class QJSEngine;

class TripStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(double distance READ distance NOTIFY distanceChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double averageSpeed READ averageSpeed NOTIFY averageSpeedChanged)

public:
    explicit TripStore(EngineStore *engine, VehicleStore *vehicle, QObject *parent = nullptr);
    static TripStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    double distance() const { return m_distance; }
    int duration() const { return m_duration; }
    double averageSpeed() const { return m_averageSpeed; }

    Q_INVOKABLE void reset();

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
    bool m_resetPending = true;  // reset on first ReadyToDrive after boot/lock
    qint64 m_accumulatedMs = 0;  // ms from completed active periods this session
    double m_distance = 0;
    int m_duration = 0;
    double m_averageSpeed = 0;

    static inline TripStore *s_instance = nullptr;
};

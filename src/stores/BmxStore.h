#pragma once

#include "SyncableStore.h"

// BmxStore subscribes to bmx-service's two pub/sub channels:
//   bmx:sensors  — 10 Hz JSON snapshot {timestamp, accel, gyro, mag}
//   bmx:heading  — 5 Hz JSON snapshot of the heading payload
//
// The base SyncableStore HGETALL safety net polls the `bmx` hash slowly so
// status fields (heading-deg, heading-accuracy, sensitivity, ...) are also
// available without a fresh push.
class BmxStore : public SyncableStore
{
    Q_OBJECT

    // Heading payload (bmx:heading)
    Q_PROPERTY(double headingDeg READ headingDeg NOTIFY headingChanged)
    Q_PROPERTY(double headingRawDeg READ headingRawDeg NOTIFY headingChanged)
    Q_PROPERTY(double accuracyDeg READ accuracyDeg NOTIFY headingChanged)
    Q_PROPERTY(bool tiltCompensated READ tiltCompensated NOTIFY headingChanged)
    Q_PROPERTY(double tiltDeg READ tiltDeg NOTIFY headingChanged)
    Q_PROPERTY(double magStrengthUT READ magStrengthUT NOTIFY headingChanged)
    Q_PROPERTY(double excessG READ excessG NOTIFY headingChanged)
    Q_PROPERTY(double yawRateDPS READ yawRateDPS NOTIFY headingChanged)
    Q_PROPERTY(qint64 headingTimestamp READ headingTimestamp NOTIFY headingChanged)

    // Sensors payload (bmx:sensors)
    Q_PROPERTY(double accelX READ accelX NOTIFY sensorsChanged)
    Q_PROPERTY(double accelY READ accelY NOTIFY sensorsChanged)
    Q_PROPERTY(double accelZ READ accelZ NOTIFY sensorsChanged)
    Q_PROPERTY(double accelMagnitude READ accelMagnitude NOTIFY sensorsChanged)
    Q_PROPERTY(double gyroX READ gyroX NOTIFY sensorsChanged)
    Q_PROPERTY(double gyroY READ gyroY NOTIFY sensorsChanged)
    Q_PROPERTY(double gyroZ READ gyroZ NOTIFY sensorsChanged)
    Q_PROPERTY(double gyroMagnitude READ gyroMagnitude NOTIFY sensorsChanged)
    Q_PROPERTY(double magX READ magX NOTIFY sensorsChanged)
    Q_PROPERTY(double magY READ magY NOTIFY sensorsChanged)
    Q_PROPERTY(double magZ READ magZ NOTIFY sensorsChanged)
    Q_PROPERTY(double magMagnitude READ magMagnitude NOTIFY sensorsChanged)
    Q_PROPERTY(qint64 sensorsTimestamp READ sensorsTimestamp NOTIFY sensorsChanged)

public:
    explicit BmxStore(MdbRepository *repo, QObject *parent = nullptr);
    ~BmxStore() override;

    void start() override;
    void stop() override;

    double headingDeg() const { return m_headingDeg; }
    double headingRawDeg() const { return m_headingRawDeg; }
    double accuracyDeg() const { return m_accuracyDeg; }
    bool tiltCompensated() const { return m_tiltCompensated; }
    double tiltDeg() const { return m_tiltDeg; }
    double magStrengthUT() const { return m_magStrengthUT; }
    double excessG() const { return m_excessG; }
    double yawRateDPS() const { return m_yawRateDPS; }
    qint64 headingTimestamp() const { return m_headingTimestamp; }

    double accelX() const { return m_accelX; }
    double accelY() const { return m_accelY; }
    double accelZ() const { return m_accelZ; }
    double accelMagnitude() const { return m_accelMagnitude; }
    double gyroX() const { return m_gyroX; }
    double gyroY() const { return m_gyroY; }
    double gyroZ() const { return m_gyroZ; }
    double gyroMagnitude() const { return m_gyroMagnitude; }
    double magX() const { return m_magX; }
    double magY() const { return m_magY; }
    double magZ() const { return m_magZ; }
    double magMagnitude() const { return m_magMagnitude; }
    qint64 sensorsTimestamp() const { return m_sensorsTimestamp; }

signals:
    void headingChanged();
    void sensorsChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    void applyHeadingSnapshot(const QString &payload);
    void applySensorsSnapshot(const QString &payload);

    bool m_headingSubscribed = false;
    bool m_sensorsSubscribed = false;

    // Heading payload state
    double m_headingDeg = 0.0;
    double m_headingRawDeg = 0.0;
    double m_accuracyDeg = 0.0;
    bool m_tiltCompensated = false;
    double m_tiltDeg = 0.0;
    double m_magStrengthUT = 0.0;
    double m_excessG = 0.0;
    double m_yawRateDPS = 0.0;
    qint64 m_headingTimestamp = 0;

    // Sensors payload state
    double m_accelX = 0.0, m_accelY = 0.0, m_accelZ = 0.0, m_accelMagnitude = 0.0;
    double m_gyroX = 0.0, m_gyroY = 0.0, m_gyroZ = 0.0, m_gyroMagnitude = 0.0;
    double m_magX = 0.0, m_magY = 0.0, m_magZ = 0.0, m_magMagnitude = 0.0;
    qint64 m_sensorsTimestamp = 0;
};

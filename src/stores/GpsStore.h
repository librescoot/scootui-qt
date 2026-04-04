#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"
#include <QElapsedTimer>

class GpsStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(double latitude READ latitude NOTIFY latitudeChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY longitudeChanged)
    Q_PROPERTY(double course READ course NOTIFY courseChanged)
    Q_PROPERTY(double speed READ speed NOTIFY speedChanged)
    Q_PROPERTY(double altitude READ altitude NOTIFY altitudeChanged)
    Q_PROPERTY(QString updated READ updated NOTIFY updatedChanged)
    Q_PROPERTY(QString timestamp READ timestamp NOTIFY timestampChanged)
    Q_PROPERTY(int gpsState READ gpsState NOTIFY gpsStateChanged)
    Q_PROPERTY(double eph READ eph NOTIFY ephChanged)
    Q_PROPERTY(bool hasRecentFix READ hasRecentFix NOTIFY timestampChanged)
    Q_PROPERTY(bool hasTimestamp READ hasTimestamp NOTIFY timestampChanged)

public:
    explicit GpsStore(MdbRepository *repo, QObject *parent = nullptr);

    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }
    double course() const { return m_course; }
    double speed() const { return m_speed; }
    double altitude() const { return m_altitude; }
    double eph() const { return m_eph; }
    QString updated() const { return m_updated; }
    QString timestamp() const { return m_timestamp; }
    int gpsState() const { return static_cast<int>(m_gpsState); }

    bool hasTimestamp() const { return !m_timestamp.isEmpty(); }
    bool hasGpsFix() const { return m_gpsState == ScootEnums::GpsState::FixEstablished; }

    // True when the GPS daemon reports fix-established AND the timestamp
    // field has been updated within the last 20 seconds (monotonic clock,
    // immune to system clock skew).
    bool hasRecentFix() const {
        return hasGpsFix() && m_timestampAge.isValid()
            && m_timestampAge.elapsed() <= RecentFixThresholdMs;
    }

    static constexpr qint64 RecentFixThresholdMs = 20000;

signals:
    void latitudeChanged();
    void longitudeChanged();
    void courseChanged();
    void speedChanged();
    void altitudeChanged();
    void updatedChanged();
    void timestampChanged();
    void gpsStateChanged();
    void ephChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    double m_latitude = 0;
    double m_longitude = 0;
    double m_course = 0;
    double m_speed = 0;
    double m_altitude = 0;
    double m_eph = 0;
    QString m_updated;
    QString m_timestamp;
    QElapsedTimer m_timestampAge;
    ScootEnums::GpsState m_gpsState = ScootEnums::GpsState::Off;
};

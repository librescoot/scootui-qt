#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"

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

public:
    explicit GpsStore(MdbRepository *repo, QObject *parent = nullptr);

    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }
    double course() const { return m_course; }
    double speed() const { return m_speed; }
    double altitude() const { return m_altitude; }
    QString updated() const { return m_updated; }
    QString timestamp() const { return m_timestamp; }
    int gpsState() const { return static_cast<int>(m_gpsState); }

signals:
    void latitudeChanged();
    void longitudeChanged();
    void courseChanged();
    void speedChanged();
    void altitudeChanged();
    void updatedChanged();
    void timestampChanged();
    void gpsStateChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    double m_latitude = 0;
    double m_longitude = 0;
    double m_course = 0;
    double m_speed = 0;
    double m_altitude = 0;
    QString m_updated;
    QString m_timestamp;
    ScootEnums::GpsState m_gpsState = ScootEnums::GpsState::Off;
};

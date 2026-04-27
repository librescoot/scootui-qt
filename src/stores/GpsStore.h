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
    Q_PROPERTY(double ept READ ept NOTIFY eptChanged)
    Q_PROPERTY(double eps READ eps NOTIFY epsChanged)
    Q_PROPERTY(double snr READ snr NOTIFY snrChanged)
    Q_PROPERTY(double pdop READ pdop NOTIFY pdopChanged)
    Q_PROPERTY(double hdop READ hdop NOTIFY hdopChanged)
    Q_PROPERTY(double vdop READ vdop NOTIFY vdopChanged)
    Q_PROPERTY(int satellitesUsed READ satellitesUsed NOTIFY satellitesUsedChanged)
    Q_PROPERTY(int satellitesVisible READ satellitesVisible NOTIFY satellitesVisibleChanged)
    Q_PROPERTY(QString fix READ fix NOTIFY fixChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(double lastTtffSeconds READ lastTtffSeconds NOTIFY lastTtffSecondsChanged)
    Q_PROPERTY(QString lastTtffMode READ lastTtffMode NOTIFY lastTtffModeChanged)
    Q_PROPERTY(bool hasRecentFix READ hasRecentFix NOTIFY timestampChanged)
    Q_PROPERTY(bool hasTimestamp READ hasTimestamp NOTIFY timestampChanged)
    Q_PROPERTY(bool hasValidGps READ hasValidGps NOTIFY hasValidGpsChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
    explicit GpsStore(MdbRepository *repo, QObject *parent = nullptr);
    ~GpsStore() override;

    void start() override;
    void stop() override;

    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }
    double course() const { return m_course; }
    double speed() const { return m_speed; }
    double altitude() const { return m_altitude; }
    double eph() const { return m_eph; }
    double ept() const { return m_ept; }
    double eps() const { return m_eps; }
    double snr() const { return m_snr; }
    double pdop() const { return m_pdop; }
    double hdop() const { return m_hdop; }
    double vdop() const { return m_vdop; }
    int satellitesUsed() const { return m_satellitesUsed; }
    int satellitesVisible() const { return m_satellitesVisible; }
    QString fix() const { return m_fix; }
    QString mode() const { return m_mode; }
    double lastTtffSeconds() const { return m_lastTtffSeconds; }
    QString lastTtffMode() const { return m_lastTtffMode; }
    QString updated() const { return m_updated; }
    QString timestamp() const { return m_timestamp; }
    int gpsState() const { return static_cast<int>(m_gpsState); }
    bool active() const { return m_active; }
    bool connected() const { return m_connected; }

    bool hasTimestamp() const { return !m_timestamp.isEmpty(); }
    bool hasGpsFix() const { return m_gpsState == ScootEnums::GpsState::FixEstablished; }

    // True when we have any non-zero coordinate. Decoupled from the gps.state
    // field, which modem-service flips to "searching" on transient TPV mode
    // 0/1 reports while leaving the last-known lat/lng in Redis untouched —
    // so the marker keeps drawing on the map but a strict gpsState check
    // would say "no GPS" for the duration of every blip.
    bool hasValidGps() const { return m_latitude != 0.0 || m_longitude != 0.0; }

    // True when the GPS daemon reports fix-established AND the timestamp
    // field has been updated within the last 20 seconds (monotonic clock,
    // immune to system clock skew).
    bool hasRecentFix() const {
        return hasGpsFix() && m_timestampAge.isValid()
            && m_timestampAge.elapsed() <= RecentFixThresholdMs;
    }

    // Milliseconds since we last saw a new GPS timestamp field. Does NOT
    // include receiver-side NMEA buffering — callers who want an estimate
    // of the fix age should add that buffer themselves.
    qint64 timestampAgeMs() const {
        return m_timestampAge.isValid() ? m_timestampAge.elapsed() : 0;
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
    void eptChanged();
    void epsChanged();
    void snrChanged();
    void pdopChanged();
    void hdopChanged();
    void vdopChanged();
    void satellitesUsedChanged();
    void satellitesVisibleChanged();
    void fixChanged();
    void modeChanged();
    void lastTtffSecondsChanged();
    void lastTtffModeChanged();
    void hasValidGpsChanged();
    void activeChanged();
    void connectedChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    void applySnapshot(const QString &payload);
    bool m_tpvSubscribed = false;
    double m_latitude = 0;
    double m_longitude = 0;
    double m_course = 0;
    double m_speed = 0;
    double m_altitude = 0;
    double m_eph = 0;
    double m_ept = 0;
    double m_eps = 0;
    double m_snr = 0;
    double m_pdop = 0;
    double m_hdop = 0;
    double m_vdop = 0;
    int m_satellitesUsed = 0;
    int m_satellitesVisible = 0;
    QString m_fix;
    QString m_mode;
    double m_lastTtffSeconds = 0;
    QString m_lastTtffMode;
    QString m_updated;
    QString m_timestamp;
    QElapsedTimer m_timestampAge;
    ScootEnums::GpsState m_gpsState = ScootEnums::GpsState::Off;
    bool m_active = false;
    bool m_connected = false;
};

#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>
#include <QPair>

class GpsStore;
class EngineStore;
class NavigationService;
class SettingsStore;
class ThemeStore;

class MapService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double mapLatitude READ mapLatitude NOTIFY mapLatitudeChanged)
    Q_PROPERTY(double mapLongitude READ mapLongitude NOTIFY mapLongitudeChanged)
    Q_PROPERTY(double mapZoom READ mapZoom NOTIFY mapZoomChanged)
    Q_PROPERTY(double mapBearing READ mapBearing NOTIFY mapBearingChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY isReadyChanged)
    Q_PROPERTY(QString styleUrl READ styleUrl NOTIFY styleUrlChanged)
    Q_PROPERTY(QVariantList routeCoordinates READ routeCoordinates NOTIFY routeCoordinatesChanged)
    Q_PROPERTY(QString routeGeoJson READ routeGeoJson NOTIFY routeGeoJsonChanged)
    Q_PROPERTY(double vehicleOffsetY READ vehicleOffsetY NOTIFY vehicleOffsetYChanged)
    Q_PROPERTY(bool isOutOfCoverage READ isOutOfCoverage NOTIFY isOutOfCoverageChanged)

public:
    explicit MapService(GpsStore *gps, EngineStore *engine,
                        NavigationService *navigation, SettingsStore *settings,
                        ThemeStore *theme, QObject *parent = nullptr);
    ~MapService() override;

    double mapLatitude() const { return m_mapLatitude; }
    double mapLongitude() const { return m_mapLongitude; }
    double mapZoom() const { return m_mapZoom; }
    double mapBearing() const { return m_mapBearing; }
    bool isReady() const { return m_isReady; }
    QString styleUrl() const { return m_styleUrl; }
    QVariantList routeCoordinates() const { return m_routeCoordinates; }
    QString routeGeoJson() const { return m_routeGeoJson; }
    double vehicleOffsetY() const { return m_vehicleOffsetY; }
    bool isOutOfCoverage() const { return m_isOutOfCoverage; }

    void setRouteWaypoints(const QVariantList &waypoints);
    void clearRoute();
    void updateRouteFromNavigation();

signals:
    void mapLatitudeChanged();
    void mapLongitudeChanged();
    void mapZoomChanged();
    void mapBearingChanged();
    void isReadyChanged();
    void styleUrlChanged();
    void routeCoordinatesChanged();
    void routeGeoJsonChanged();
    void vehicleOffsetYChanged();
    void isOutOfCoverageChanged();

private slots:
    void onDeadReckoningTick();
    void onGpsPositionChanged();
    void onRouteChanged();
    void onThemeChanged();
    void onMapTypeChanged();
    void onOverviewTimeout();

private:
    // Dead reckoning
    void projectPositionAlongRoute(double distMeters);
    void projectPositionStraight(double distMeters, double headingDeg);
    void blendGpsCorrection(double dt);
    void applyLatencyCompensation(double speedMs, double headingDeg);

    // Dynamic zoom
    void updateDynamicZoom(double dt);
    double computeTargetZoom() const;
    double distanceToNextManeuver() const;
    double distanceToSecondManeuver() const;

    // Rotation smoothing
    void updateBearing(double dt);
    static double normalizeAngle(double angle);

    // Style
    void rebuildStyleUrl();
    QString rewriteStyleForMbtiles(const QString &qrcPath, const QString &mbtilesPath);

    // Route GeoJSON for native MapLibre layer
    void updateRouteGeoJson();

    // Coverage bounds checking
    void loadMbtilesBounds();
    void checkOutOfCoverage();

    // Snap to closest route segment and return index, or -1 if off route
    int findClosestSegment(double lat, double lng) const;

    // Bearing along the current route segment, or -1 if not on route
    double routeSegmentBearing() const;

    // --- Constants ---

    // Dead reckoning
    static constexpr double TickIntervalMs = 66.0;
    static constexpr double SpeedFactor = 0.95;
    static constexpr double LatencyCompensationSec = 0.15;
    static constexpr double BlendRateNormal = 2.0;
    static constexpr double BlendRateLarge = 5.0;
    static constexpr double SnapThreshold = 50.0;
    static constexpr double SnapUpperThreshold = 500.0;
    static constexpr double SnapAnimationDuration = 1.0;
    static constexpr double LargeErrorThreshold = 15.0;

    // Dynamic zoom
    static constexpr double DefaultZoom = 16.0;
    static constexpr double MinZoom = 15.0;
    static constexpr double MaxZoom = 17.5;
    static constexpr double ZoomHysteresis = 0.3;
    static constexpr double ZoomSmoothRate = 1.0;
    static constexpr double MultiTurnLookAheadMeters = 150.0;

    // Route overview (zoom out briefly after route calculation)
    static constexpr double OverviewZoom = 15.0;
    static constexpr double OverviewZoomRate = 2.0;
    static constexpr int OverviewHoldMs = 3000;

    // Rotation smoothing
    static constexpr double HeadingFreezeSpeed = 1.0;    // km/h
    static constexpr double HeadingFullSpeed = 10.0;     // km/h
    static constexpr double TargetSmoothRate = 8.0;      // exponential blend rate per second
    static constexpr double MaxBearingRate = 110.0;       // deg/sec max approach speed
    static constexpr double RotationAnimDuration = 1.0;   // seconds to complete rotation

    // Vehicle offset
    static constexpr double VehicleOffsetPx = 120.0;

    // --- Store pointers ---
    GpsStore *m_gps;
    EngineStore *m_engine;
    NavigationService *m_navigation;
    SettingsStore *m_settings;
    ThemeStore *m_theme;

    // --- Mbtiles path (resolved at construction) ---
    QString m_mbtilesPath;

    // --- Timers ---
    QTimer *m_tickTimer;
    QElapsedTimer m_elapsed;

    // --- Camera state ---
    double m_mapLatitude = 0;
    double m_mapLongitude = 0;
    double m_mapZoom = DefaultZoom;
    double m_mapBearing = 0;
    bool m_isReady = false;
    QString m_styleUrl;
    QVariantList m_routeCoordinates;
    QString m_routeGeoJson;
    double m_vehicleOffsetY = VehicleOffsetPx;

    // --- Out-of-coverage state ---
    bool m_isOutOfCoverage = false;
    bool m_hasBounds = false;
    double m_boundsMinLat = 0;
    double m_boundsMaxLat = 0;
    double m_boundsMinLng = 0;
    double m_boundsMaxLng = 0;

    // --- Dead reckoning state ---
    double m_drLatitude = 0;
    double m_drLongitude = 0;
    double m_lastGpsLatitude = 0;
    double m_lastGpsLongitude = 0;
    bool m_hasInitialPosition = false;
    int m_currentRouteSegment = -1;

    // GPS correction blending
    double m_gpsErrorLatitude = 0;
    double m_gpsErrorLongitude = 0;

    // Snap animation
    bool m_isSnapping = false;
    double m_snapProgress = 0;
    double m_snapStartLat = 0;
    double m_snapStartLng = 0;
    double m_snapTargetLat = 0;
    double m_snapTargetLng = 0;

    // --- Route shape for dead reckoning ---
    QList<QPair<double, double>> m_routeShape; // (lat, lng) pairs

    // --- Dynamic zoom state ---
    double m_targetZoom = DefaultZoom;
    double m_currentZoom = DefaultZoom;

    // --- Route overview state ---
    QTimer *m_overviewTimer = nullptr;
    bool m_routeOverviewActive = false;

    // --- Rotation smoothing state ---
    double m_smoothedTarget = 0;
    double m_displayBearing = 0;
    double m_lastRawHeading = 0;
};

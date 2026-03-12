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
    Q_PROPERTY(double mapTilt READ mapTilt NOTIFY mapTiltChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY isReadyChanged)
    Q_PROPERTY(QString styleUrl READ styleUrl NOTIFY styleUrlChanged)
    Q_PROPERTY(QVariantList routeCoordinates READ routeCoordinates NOTIFY routeCoordinatesChanged)
    Q_PROPERTY(double vehicleOffsetY READ vehicleOffsetY NOTIFY vehicleOffsetYChanged)

public:
    explicit MapService(GpsStore *gps, EngineStore *engine,
                        NavigationService *navigation, SettingsStore *settings,
                        ThemeStore *theme, QObject *parent = nullptr);
    ~MapService() override;

    double mapLatitude() const { return m_mapLatitude; }
    double mapLongitude() const { return m_mapLongitude; }
    double mapZoom() const { return m_mapZoom; }
    double mapBearing() const { return m_mapBearing; }
    double mapTilt() const { return m_mapTilt; }
    bool isReady() const { return m_isReady; }
    QString styleUrl() const { return m_styleUrl; }
    QVariantList routeCoordinates() const { return m_routeCoordinates; }
    double vehicleOffsetY() const { return m_vehicleOffsetY; }

    void setRouteWaypoints(const QVariantList &waypoints);
    void clearRoute();
    void updateRouteFromNavigation();

signals:
    void mapLatitudeChanged();
    void mapLongitudeChanged();
    void mapZoomChanged();
    void mapBearingChanged();
    void mapTiltChanged();
    void isReadyChanged();
    void styleUrlChanged();
    void routeCoordinatesChanged();
    void vehicleOffsetYChanged();

private slots:
    void onDeadReckoningTick();
    void onGpsPositionChanged();
    void onRouteChanged();
    void onThemeChanged();
    void onMapTypeChanged();

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
    double computeRouteOverviewZoom() const;

    // Rotation smoothing
    void updateBearing(double dt);
    static double normalizeAngle(double angle);

    // Overview blend (0 = normal nav, 1 = full overview)
    double computeOverviewBlend() const;

    // Style
    void rebuildStyleUrl();
    QString rewriteStyleForMbtiles(const QString &qrcPath, const QString &mbtilesPath);

    // Snap to closest route segment and return index, or -1 if off route
    int findClosestSegment(double lat, double lng) const;

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
    static constexpr double OverviewMinZoom = 5.0;
    static constexpr double ZoomHysteresis = 0.3;
    static constexpr double ZoomSmoothRate = 1.0;
    static constexpr double ZoomSmoothRateOverview = 3.0;
    static constexpr double MultiTurnLookAheadMeters = 150.0;
    static constexpr double RouteOverviewDurationMs = 5000.0;
    static constexpr double NavigationTilt = 85.0;

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

    // --- Timers ---
    QTimer *m_tickTimer;
    QElapsedTimer m_elapsed;

    // --- Camera state ---
    double m_mapLatitude = 0;
    double m_mapLongitude = 0;
    double m_mapZoom = DefaultZoom;
    double m_mapBearing = 0;
    double m_mapTilt = NavigationTilt;
    bool m_isReady = false;
    QString m_styleUrl;
    QVariantList m_routeCoordinates;
    double m_vehicleOffsetY = VehicleOffsetPx;

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
    bool m_inRouteOverview = false;
    double m_navZoomAtOverview = DefaultZoom;
    double m_overviewTargetZoom = DefaultZoom;

    // --- Rotation smoothing state ---
    double m_smoothedTarget = 0;
    double m_displayBearing = 0;
    double m_lastRawHeading = 0;
};

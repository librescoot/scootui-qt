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
class SpeedLimitStore;

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
    Q_PROPERTY(bool deadReckoningPaused READ deadReckoningPaused WRITE setDeadReckoningPaused NOTIFY deadReckoningPausedChanged)
    Q_PROPERTY(double vehicleLatitude READ vehicleLatitude NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleLongitude READ vehicleLongitude NOTIFY vehiclePositionChanged)
    Q_PROPERTY(bool hasVehiclePosition READ hasVehiclePosition NOTIFY vehiclePositionChanged)
    Q_PROPERTY(int currentRouteSegment READ currentRouteSegment NOTIFY routeProjectionChanged)
    Q_PROPERTY(double snappedLatitude READ snappedLatitude NOTIFY routeProjectionChanged)
    Q_PROPERTY(double snappedLongitude READ snappedLongitude NOTIFY routeProjectionChanged)
    Q_PROPERTY(double segmentSnappedLatitude READ segmentSnappedLatitude NOTIFY routeProjectionChanged)
    Q_PROPERTY(double segmentSnappedLongitude READ segmentSnappedLongitude NOTIFY routeProjectionChanged)
    Q_PROPERTY(double distanceFromRoute READ distanceFromRoute NOTIFY routeProjectionChanged)

public:
    explicit MapService(GpsStore *gps, EngineStore *engine,
                        NavigationService *navigation, SettingsStore *settings,
                        ThemeStore *theme, SpeedLimitStore *speedLimit,
                        QObject *parent = nullptr);
    ~MapService() override;

    void reloadMbtiles();

    bool deadReckoningPaused() const { return m_deadReckoningPaused; }
    void setDeadReckoningPaused(bool paused) { if (paused != m_deadReckoningPaused) { m_deadReckoningPaused = paused; emit deadReckoningPausedChanged(); } }

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

    double vehicleLatitude() const { return m_drLatitude; }
    double vehicleLongitude() const { return m_drLongitude; }
    bool hasVehiclePosition() const { return m_hasInitialPosition; }

    // Route-projection state — authoritative source for NavigationService.
    // currentRouteSegment is the index of the polyline segment the rider is
    // matched to (not just geometrically nearest — trajectory-aware, so
    // opposite-direction close-by segments don't pull us back on U-turns).
    // -1 when no route or off-route.
    // snappedLatitude/Longitude = global-nearest projection across all route
    // segments (used for off-route hysteresis and the distFromRoute value).
    // segmentSnappedLatitude/Longitude = projection onto the matcher's
    // current segment specifically (used for TBT along-route distance).
    int currentRouteSegment() const { return m_currentRouteSegment; }
    double snappedLatitude() const { return m_snappedLat; }
    double snappedLongitude() const { return m_snappedLng; }
    double segmentSnappedLatitude() const { return m_segmentSnappedLat; }
    double segmentSnappedLongitude() const { return m_segmentSnappedLng; }
    double distanceFromRoute() const { return m_distFromRoute; }

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
    void deadReckoningPausedChanged();
    void vehiclePositionChanged();
    void routeProjectionChanged();

private slots:
    void onDeadReckoningTick();
    void onGpsPositionChanged();
    void onRouteChanged();
    void onThemeChanged();
    void onMapTypeChanged();
    void onTrafficOverlayChanged();
    void onOverviewTimeout();

private:
    // Dead reckoning
    void projectPositionAlongRoute(double distMeters);
    void projectPositionStraight(double distMeters, double headingDeg);
    void blendGpsCorrection(double dt);
    void snapToRouteLine();

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

    // Traffic overlay
    static void removeTrafficFromStyle(QJsonObject &root);
    QString rewriteStyleStripTraffic(const QString &qrcPath);

    // Route GeoJSON for native MapLibre layer
    void updateRouteGeoJson();

    // Coverage bounds checking
    void loadMbtilesBounds();
    void checkOutOfCoverage();

    // Trajectory-aware segment matcher. Combines perpendicular distance,
    // direction-of-travel alignment, and a hysteresis bias toward the current
    // segment. On U-turns / sharp turns the direction penalty keeps us from
    // snapping back to a geometrically-nearer opposite-direction segment.
    // Returns {-1, 0, 0, 0} when no valid match (no route, off-route, etc).
    struct SegmentMatch {
        int index = -1;
        double snappedLat = 0;
        double snappedLng = 0;
        double perpDist = 0;       // m
        double cost = 0;           // combined (distance + direction + hysteresis)
    };
    SegmentMatch matchRouteSegment(double lat, double lng,
                                    double trajectoryBearing,
                                    bool haveTrajectory,
                                    int currentSegment) const;

    // Recompute m_snappedLat/Lng/m_distFromRoute from current DR position
    // and m_currentRouteSegment. Emits routeProjectionChanged if values moved.
    void refreshRouteProjection();

    // Bearing along the current route segment, or -1 if not on route
    double routeSegmentBearing() const;

    // --- Constants ---

    // Dead reckoning
    static constexpr double TickIntervalMs = 66.0;
    // Output-side compensation covers only render-path latency now that the
    // GPS sample itself is age-corrected on input. Was 0.15 s.
    static constexpr double LatencyCompensationSec = 0.05;
    // SIMcom NMEA receiver buffer — approximate fix age between receiver
    // measurement and modem-service publish. Added to the consumer-side age
    // from GpsStore::timestampAgeMs to estimate the actual fix age.
    static constexpr double GpsReceiverBufferMs = 300.0;

    // Odometer-primary DR: odometer (100 m steps, meters) is the truth.
    // Speed acts as feedforward between odometer edges; a bounded catchup
    // term closes the gap between cumulative DR distance and odometer.
    static constexpr double CatchupRate = 0.5;         // /s — closes half of deficit per second
    static constexpr double MaxCatchupPerTick = 0.5;   // meters — clamp per-tick correction
    static constexpr double StationarySpeedMs = 0.3;   // below this, assume no motion

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

    // Trajectory-aware segment matching
    static constexpr int MatchWindowBack = 10;
    static constexpr int MatchWindowFwd = 10;
    static constexpr double MatchAcceptanceDistance = 120.0;   // m — beyond this, don't even try
    static constexpr double MinSpeedForTrajectoryKmh = 3.0;    // below this, direction is unreliable
    static constexpr double ReverseDirectionPenalty = 50.0;    // m-equivalent for > 90° mismatch
    static constexpr double ReverseSlopePerDeg = 0.5;          // m/deg above 90°
    static constexpr double SoftDirectionFactor = 0.15;        // m/deg ≤ 90°
    static constexpr double CurrentSegmentBonus = 5.0;         // m handicap to current (prefer stability)
    static constexpr double BackwardStepPenalty = 3.0;         // m/step for going backward
    static constexpr double ForwardStepPenalty = 0.5;          // m/step for skipping ahead
    static constexpr double SwitchHysteresis = 2.0;            // new must beat current by this much
    static constexpr double SnappedPosEpsilon = 0.5;           // m — don't emit below this

    // Last-emitted projection state, for change detection on routeProjectionChanged
    mutable double m_lastEmittedSnapLat = 0;
    mutable double m_lastEmittedSnapLng = 0;
    mutable double m_lastEmittedDistFromRoute = -1;
    mutable int m_lastEmittedSegment = -2;

    // --- Store pointers ---
    GpsStore *m_gps;
    EngineStore *m_engine;
    NavigationService *m_navigation;
    SettingsStore *m_settings;
    ThemeStore *m_theme;
    SpeedLimitStore *m_speedLimit;

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
    bool m_deadReckoningPaused = false;
    int m_currentRouteSegment = -1;
    // High-water mark — furthest segment index the rider has reached on the
    // current route shape. The matcher is gated against regressing below it;
    // forward jumps (shortcuts) are still allowed. Reset on new route / clear.
    int m_maxReachedSegment = -1;

    // Route-projection state (exposed to NavigationService via Q_PROPERTY)
    double m_snappedLat = 0;
    double m_snappedLng = 0;
    double m_distFromRoute = 0;
    double m_segmentSnappedLat = 0;
    double m_segmentSnappedLng = 0;

    // Odometer-primary DR bookkeeping
    bool m_odoSeeded = false;
    double m_odoAtSeed = 0;       // meters — odometer snapshot at seed
    double m_odoTarget = 0;       // meters since seed per odometer
    double m_drTravelled = 0;     // meters since seed per our integration

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

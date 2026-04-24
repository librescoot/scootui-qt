#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include "routing/RouteModels.h"
#include "routing/ValhallaClient.h"

class GpsStore;
class NavigationStore;
class VehicleStore;
class SettingsStore;
class SpeedLimitStore;
class MdbRepository;
class MapService;

class NavigationService : public QObject
{
    Q_OBJECT

    // Navigation status
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool isNavigating READ isNavigating NOTIFY statusChanged)
    Q_PROPERTY(bool isRerouting READ isRerouting NOTIFY statusChanged)
    Q_PROPERTY(bool hasRoute READ hasRoute NOTIFY routeChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

    // Destination
    Q_PROPERTY(double destLatitude READ destLatitude NOTIFY destinationChanged)
    Q_PROPERTY(double destLongitude READ destLongitude NOTIFY destinationChanged)
    Q_PROPERTY(QString destAddress READ destAddress NOTIFY destinationChanged)

    // Current instruction
    Q_PROPERTY(int currentManeuverType READ currentManeuverType NOTIFY instructionChanged)
    Q_PROPERTY(double currentManeuverDistance READ currentManeuverDistance NOTIFY instructionChanged)
    Q_PROPERTY(QString currentStreetName READ currentStreetName NOTIFY instructionChanged)
    Q_PROPERTY(QString currentVerbalInstruction READ currentVerbalInstruction NOTIFY instructionChanged)
    Q_PROPERTY(QString currentInstructionText READ currentInstructionText NOTIFY instructionChanged)

    // Next instruction (preview)
    Q_PROPERTY(int nextManeuverType READ nextManeuverType NOTIFY instructionChanged)
    Q_PROPERTY(double nextManeuverDistance READ nextManeuverDistance NOTIFY instructionChanged)
    Q_PROPERTY(QString nextStreetName READ nextStreetName NOTIFY instructionChanged)
    Q_PROPERTY(bool hasNextInstruction READ hasNextInstruction NOTIFY instructionChanged)
    Q_PROPERTY(bool showNextPreview READ showNextPreview NOTIFY instructionChanged)

    // Trip summary
    Q_PROPERTY(double distanceToDestination READ distanceToDestination NOTIFY positionChanged)
    Q_PROPERTY(double distanceFromRoute READ distanceFromRoute NOTIFY positionChanged)
    Q_PROPERTY(bool isOffRoute READ isOffRoute NOTIFY positionChanged)
    Q_PROPERTY(double totalDistance READ totalDistance NOTIFY routeChanged)
    Q_PROPERTY(double totalDuration READ totalDuration NOTIFY routeChanged)
    Q_PROPERTY(double remainingDuration READ remainingDuration NOTIFY positionChanged)
    Q_PROPERTY(QString eta READ eta NOTIFY positionChanged)

    // Roundabout
    Q_PROPERTY(int roundaboutExitCount READ roundaboutExitCount NOTIFY instructionChanged)

public:
    explicit NavigationService(GpsStore *gps, NavigationStore *nav,
                                VehicleStore *vehicle, SettingsStore *settings,
                                SpeedLimitStore *speedLimit, MdbRepository *repo,
                                QObject *parent = nullptr);

    int status() const { return static_cast<int>(m_status); }
    bool isNavigating() const { return m_status == NavigationStatus::Navigating; }
    bool isRerouting() const { return m_status == NavigationStatus::Rerouting; }
    bool hasRoute() const { return m_route.isValid(); }
    QString errorMessage() const { return m_errorMessage; }

    double destLatitude() const { return m_destination.latitude; }
    double destLongitude() const { return m_destination.longitude; }
    QString destAddress() const { return m_destAddress; }

    int currentManeuverType() const;
    double currentManeuverDistance() const;
    QString currentStreetName() const;
    QString currentVerbalInstruction() const;
    QString currentInstructionText() const;
    int roundaboutExitCount() const;

    int nextManeuverType() const;
    double nextManeuverDistance() const;
    QString nextStreetName() const;
    bool hasNextInstruction() const;
    bool showNextPreview() const;

    double distanceToDestination() const { return m_distanceToDestination; }
    double distanceFromRoute() const { return m_distanceFromRoute; }
    bool isOffRoute() const { return m_isOffRoute; }
    double totalDistance() const { return m_route.distance; }
    double totalDuration() const { return m_route.duration; }
    double remainingDuration() const;
    QString eta() const;

    // 1.20 for a local Valhalla whose tiles pre-date the default_speeds.json
    // rollout (see NavigationService.cpp). 1.0 for a remote endpoint or
    // post-rollout tiles. Default-to-pad when /status hasn't succeeded yet.
    double durationPadFactor() const;

    Q_INVOKABLE void setDestination(double lat, double lng, const QString &address = {});
    Q_INVOKABLE void clearNavigation();
    Q_INVOKABLE void setRoute(const Route &route);

    // Wire the MapService after both are constructed (resolves the
    // nav↔map circular dependency). NavigationService subscribes to
    // vehiclePositionChanged to keep TBT in sync with dead reckoning.
    void setMapService(MapService *map);

    // Route waypoints for MapService dead reckoning
    QList<LatLng> routeWaypoints() const { return m_route.waypoints; }

signals:
    void statusChanged();
    void routeChanged();
    void errorChanged();
    void destinationChanged();
    void instructionChanged();
    void positionChanged();

private slots:
    void onGpsChanged();
    void onNavigationDataChanged();
    void onVehicleStateChanged();
    void onRouteCalculated(const Route &route);
    void onRouteError(const QString &error);
    void onRequestRejected(ValhallaClient::Reason reason,
                           ValhallaClient::RejectionCause cause);
    void onVehiclePositionChanged();

private:
    void updateNavigationState();
    void updateVerbalStage(const RouteInstruction &first);
    void updateNextPreviewState();
    void setStatus(NavigationStatus status);
    LatLng currentPosition() const;     // DR position when available, else raw GPS
    LatLng currentGpsPosition() const;  // raw GPS only (for rerouting)
    bool hasValidGps() const;

    // Thresholds (meters)
    static constexpr double ArrivalProximity = 50.0;
    static constexpr double OffRouteTolerance = 60.0;
    static constexpr double OnRouteTolerance = 35.0;
    static constexpr double ShutdownProximity = 250.0;

    // Verbal-instruction stage thresholds. Hysteresis bands prevent
    // text-flip oscillation when DR distance jitters around a boundary.
    static constexpr double VerbalAlertEnter   = 310.0;   // pre -> alert when distance >= this
    static constexpr double VerbalAlertExit    = 290.0;   // alert -> pre when distance <= this
    static constexpr double VerbalSuccinctEnter = 47.0;   // pre -> succinct when distance <= this
    static constexpr double VerbalSuccinctExit  = 53.0;   // succinct -> pre when distance >= this
    static constexpr double NextPreviewShow    = 290.0;   // show preview when next distance <= this
    static constexpr double NextPreviewHide    = 310.0;   // hide preview when next distance >= this

    GpsStore *m_gps;
    NavigationStore *m_nav;
    VehicleStore *m_vehicle;
    SettingsStore *m_settings;
    SpeedLimitStore *m_speedLimit;
    MdbRepository *m_repo;
    ValhallaClient *m_valhalla;
    MapService *m_map = nullptr;

    // Throttle DR-driven nav updates; the 15 Hz DR tick is too fast for the
    // route snapping + upcoming-instruction walk, and QML bindings churn
    // faster than they can be meaningfully consumed.
    QElapsedTimer m_lastDrUpdate;
    static constexpr int DrUpdateMinIntervalMs = 200;  // 5 Hz

    NavigationStatus m_status = NavigationStatus::Idle;
    Route m_route;
    LatLng m_destination;
    QString m_destAddress;
    QString m_errorMessage;

    QList<RouteInstruction> m_upcomingInstructions;
    double m_distanceToDestination = 0;
    double m_remainingDuration = 0;
    double m_distanceFromRoute = 0;
    bool m_isOffRoute = false;
    LatLng m_snappedPosition;
    int m_currentSegmentIndex = 0;

    bool m_wasArrived = false;

    // TBT stage hysteresis: reset whenever the leading maneuver changes.
    //   0 = alert (>300m), 1 = pre-transition, 2 = succinct (<=50m)
    int m_currentVerbalStage = 0;
    int m_currentVerbalInstrShapeIdx = -1;
    bool m_nextPreviewShown = false;
    int m_nextPreviewInstrShapeIdx = -1;

    QTimer *m_navDataDebounce = nullptr;
};

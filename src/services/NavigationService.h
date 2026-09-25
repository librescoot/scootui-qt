#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantMap>
#include "routing/ReroutePolicy.h"
#include "routing/RouteModels.h"
#include "routing/ValhallaClient.h"
#include "services/NavigationCadence.h"
#include "services/RoutePlanRpc.h"
#include <QJsonObject>
#include <QHash>

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
    Q_PROPERTY(bool isWaitingForPosition READ isWaitingForPosition NOTIFY statusChanged)
    Q_PROPERTY(bool hasRoute READ hasRoute NOTIFY routeChanged)
    Q_PROPERTY(bool canAvoidRoad READ canAvoidRoad NOTIFY positionChanged)
    Q_PROPERTY(bool blockedRoadActive READ blockedRoadActive NOTIFY blockedRoadChanged)
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
    Q_PROPERTY(QString currentCompactInstruction READ currentCompactInstruction NOTIFY instructionChanged)
    Q_PROPERTY(bool currentIsStart READ currentIsStart NOTIFY instructionChanged)
    Q_PROPERTY(bool currentIsArrive READ currentIsArrive NOTIFY instructionChanged)
    Q_PROPERTY(bool hasCurrentManeuver READ hasCurrentManeuver NOTIFY instructionChanged)

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
    // Deliberately not NOTIFY instructionChanged: that fires at the navigation
    // cadence because RouteInstruction compares its distance, and this payload
    // does not depend on where the rider is. It only changes when the
    // Enter/Exit pair does.
    Q_PROPERTY(QVariantMap currentRoundaboutRender READ currentRoundaboutRender
               NOTIFY roundaboutRenderChanged)

    // Multi-hop plan. planState carries RoutePlanState as an int; QML compares
    // against the same constants NavigationStatus uses. hopPromptVisible is
    // true during the drive-mode countdown at an intermediate stop.
    Q_PROPERTY(bool hasPlan READ hasPlan NOTIFY planChanged)
    Q_PROPERTY(int planState READ planState NOTIFY planStateChanged)
    Q_PROPERTY(QVariantList planStops READ planStops NOTIFY planChanged)
    Q_PROPERTY(int currentStep READ currentStep NOTIFY planChanged)
    Q_PROPERTY(int stopCount READ stopCount NOTIFY planChanged)
    Q_PROPERTY(QString nextStopLabel READ nextStopLabel NOTIFY planChanged)
    Q_PROPERTY(bool hopPromptVisible READ hopPromptVisible NOTIFY hopPromptChanged)
    Q_PROPERTY(bool hopParkedNoticeVisible READ hopParkedNoticeVisible NOTIFY planStateChanged)
    Q_PROPERTY(int hopPromptSecondsRemaining READ hopPromptSecondsRemaining
               NOTIFY hopPromptChanged)
    Q_PROPERTY(int hopPromptTimeoutSeconds READ hopPromptTimeoutSeconds CONSTANT)
    // Per-hop overview of the remaining plan, from the current position to the
    // last stop. Each entry: fromIndex (-1 for the current position), toIndex,
    // fromLabel, toLabel, distance meters, duration seconds, ready. Entries are
    // present but not ready until the preview request answers.
    Q_PROPERTY(QVariantList planOverview READ planOverview NOTIFY planOverviewChanged)
    Q_PROPERTY(double planTotalDistance READ planTotalDistance NOTIFY planOverviewChanged)
    Q_PROPERTY(double planTotalDuration READ planTotalDuration NOTIFY planOverviewChanged)

public:
    explicit NavigationService(GpsStore *gps, NavigationStore *nav,
                                VehicleStore *vehicle, SettingsStore *settings,
                                SpeedLimitStore *speedLimit, MdbRepository *repo,
                                QObject *parent = nullptr);

    int status() const { return static_cast<int>(m_status); }
    bool isNavigating() const { return m_status == NavigationStatus::Navigating; }
    bool isRerouting() const { return m_status == NavigationStatus::Rerouting; }
    // True while a route request is held because no position is accurate
    // enough to route from. The request is retried automatically; this exists
    // so the UI can show why the route has not appeared yet.
    bool isWaitingForPosition() const {
        return m_status == NavigationStatus::WaitingForPosition;
    }
    bool hasRoute() const { return m_route.isValid(); }
    bool canAvoidRoad() const;
    bool blockedRoadActive() const { return m_blockedLocation.isValid(); }
    bool lastRouteWasReroute() const {
        return m_activeRouteReason == ValhallaClient::Reason::Reroute;
    }
    QString errorMessage() const { return m_errorMessage; }

    double destLatitude() const { return m_destination.latitude; }
    double destLongitude() const { return m_destination.longitude; }
    QString destAddress() const { return m_destAddress; }

    int currentManeuverType() const;
    double currentManeuverDistance() const;
    QString currentStreetName() const;
    // Street name of the road we're CURRENTLY on (most recent passed
    // maneuver), as opposed to currentStreetName() which is the road we'll
    // enter at the next maneuver. Used by RoadInfoService to bias the road
    // name / speed limit lookup against the route — without this, an
    // overlapping tunnel/bridge way at the same lat/lon can win the
    // nearest-segment race against the surface road we're actually on.
    QString currentSegmentStreetName() const;
    int currentSegmentIndex() const { return m_currentSegmentIndex; }

    // Per-edge metadata for the segment we're currently on, sourced from a
    // Valhalla /trace_attributes follow-up. Returns false / empty defaults
    // if trace_attributes hasn't landed yet (or failed) — RoadInfoService
    // falls back to the tile path (with Layer 1's name-bias) in that case.
    bool hasCurrentEdgeAttrs() const;
    QString currentEdgeName() const;          // names[0] (street name)
    QStringList currentEdgeRefs() const;      // names[1..] (e.g. {"B 2", "B 5"})
    QString currentEdgeRoadClass() const;     // motorway / primary / ...
    int     currentEdgeSpeedLimitKph() const; // 0 = no posted limit
    bool    currentEdgeIsTunnel() const;
    bool    currentEdgeIsBridge() const;

    QString currentVerbalInstruction() const;
    QString currentInstructionText() const;
    QString currentCompactInstruction() const;
    bool currentIsStart() const;
    bool currentIsArrive() const;
    bool hasCurrentManeuver() const { return !m_upcomingInstructions.isEmpty(); }
    int roundaboutExitCount() const;
    QVariantMap currentRoundaboutRender() const;

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

    bool hasPlan() const { return m_plan.isValid(); }
    int planState() const { return static_cast<int>(m_planState); }
    QVariantList planStops() const;
    int currentStep() const { return m_plan.currentStep; }
    int stopCount() const { return m_plan.stopCount(); }
    QString nextStopLabel() const { return m_plan.nextStop().label; }
    bool hopPromptVisible() const { return m_planState == RoutePlanState::AtStop; }
    bool hopParkedNoticeVisible() const {
        return m_planState == RoutePlanState::Paused && m_pausedAfterReach;
    }
    int hopPromptSecondsRemaining() const { return m_hopSecondsLeft; }
    int hopPromptTimeoutSeconds() const { return HopAdvanceTimeoutSeconds; }
    QVariantList planOverview() const { return m_planOverview; }
    double planTotalDistance() const { return m_planTotalDistance; }
    double planTotalDuration() const { return m_planTotalDuration; }
    // Merged preview geometry for the remaining plan, for framing the overview.
    // Empty until the preview request answers.
    QList<LatLng> planGeometryWaypoints() const { return m_planGeometry; }

    // 1.20 for a local Valhalla whose tiles pre-date the default_speeds.json
    // rollout (see NavigationService.cpp). 1.0 for a remote endpoint or
    // post-rollout tiles. Default-to-pad when /status hasn't succeeded yet.
    double durationPadFactor() const;

    Q_INVOKABLE void setDestination(double lat, double lng, const QString &address = {});
    Q_INVOKABLE void clearNavigation();
    Q_INVOKABLE void avoidRoadAhead();
    Q_INVOKABLE void clearRoadAvoidance();
    Q_INVOKABLE void setRoute(const Route &route);

    // Replace the plan and start guiding to stops[startStep]. Registered stops
    // before startStep are marked reached. An empty list clears navigation.
    // The QVariantList overload takes a list of maps with latitude/longitude
    // (or lat/lon) and an optional label, which is how QML and the wire format
    // express stops.
    Q_INVOKABLE void setRoutePlan(const QVariantList &stops, int startStep = 0);
    void setRoutePlan(const QList<RouteStop> &stops, int startStep = 0);
    Q_INVOKABLE void appendStop(double lat, double lng, const QString &label = {});
    Q_INVOKABLE void removeStop(int index);
    Q_INVOKABLE void moveStop(int from, int to);
    // Advance to the next hop now, from any guided or held state. On the last
    // stop this completes and clears the plan.
    Q_INVOKABLE void skipCurrentStop();
    // Re-target the plan to stops[index]: earlier stops are marked reached and
    // guidance restarts on that hop.
    Q_INVOKABLE void jumpToStop(int index);
    Q_INVOKABLE void confirmContinue();
    Q_INVOKABLE void declineContinue();
    Q_INVOKABLE void keepCurrentStop();
    Q_INVOKABLE void setHopMenuOpen(bool open);
    Q_INVOKABLE void pausePlan();
    Q_INVOKABLE void resumePlan();

    // Wire the MapService after both are constructed (resolves the
    // nav↔map circular dependency). NavigationService subscribes to
    // vehiclePositionChanged to keep TBT in sync with dead reckoning.
    void setMapService(MapService *map);

    // Route waypoints for MapService dead reckoning
    QList<LatLng> routeWaypoints() const { return m_route.waypoints; }

signals:
    void statusChanged();
    void arrived();
    void arrivalReset();
    void routeChanged();
    void blockedRoadChanged();
    void routeAttributesChanged();
    void errorChanged();
    void destinationChanged();
    // Fired once every time a user explicitly requests navigation to a
    // point. Unlike destinationChanged (which is also emitted on route
    // recalculation), this carries the full triple so recent-destinations
    // bookkeeping can stay decoupled from the service itself.
    void destinationRequested(double latitude, double longitude, const QString &label);
    void instructionChanged();
    void positionChanged();
    void roundaboutRenderChanged();

    void planChanged();
    void planStateChanged();
    // Navigation ended without reaching the destination: rider stop, external
    // clear, or leaving early.
    void navigationStopped();
    // Fired after a persisted plan is restored on startup. Delayed a turn so
    // listeners connected during Application construction still see it.
    void planRestored(const QString &label, int step, int stopCount);
    // Fired when an intermediate stop is reached, before the continue prompt.
    // Distinct from arrived(), which stays reserved for the final stop.
    void hopReached(int step, const QString &label);
    void hopPromptChanged();
    void planOverviewChanged();

private slots:
    void onGpsChanged();
    void onNavigationDataChanged();
    void onVehicleStateChanged();
    void onRouteCalculated(const Route &route);
    void onRouteAttributesReady(const QList<EdgeAttrs> &attrs);
    void onRouteError(const QString &error);
    void onRequestRejected(ValhallaClient::Reason reason,
                           ValhallaClient::RejectionCause cause);
    void onRequestDispatched(ValhallaClient::Reason reason);
    void onVehiclePositionChanged();
    void onPlanPreviewReady(const QList<Route> &legs);
    void onPlanPreviewFailed(const QString &error);

private:
    void updateNavigationState();
    // Rebuilds m_roundaboutRender when the Enter/Exit pair changes, and emits
    // roundaboutRenderChanged only then.
    void updateRoundaboutRender();
    QVariantMap buildRoundaboutRender(int enterInstrIdx, int exitInstrIdx) const;
    void updateVerbalStage(const RouteInstruction &first);
    void updateNextPreviewState();
    void setStatus(NavigationStatus status);
    // Raise a user-visible routing error: sets the message, flips to Error and
    // arms the linger timer. Error is otherwise only left again by a
    // successful route or a new destination, so a failure with no route to
    // fall back on would pin the error pill over the map indefinitely.
    void raiseError(const QString &message);
    // Drop the error state again, either because the linger expired or because
    // the underlying condition resolved.
    void clearError();
    LatLng currentPosition() const;     // DR position when available, else raw GPS
    LatLng currentGpsPosition() const;
    RouteOrigin selectRouteOrigin() const;
    bool requestRoute(ValhallaClient::Reason reason);
    // Hold a route request that cannot be dispatched yet because no origin
    // passes the accuracy gates, instead of dropping it. The request is
    // retried from onGpsChanged()/onVehiclePositionChanged() as soon as an
    // origin becomes usable.
    void deferRouteForPosition(ValhallaClient::Reason reason);
    // Re-issue a held request once an origin is usable. No-op when nothing is
    // pending or the plan has since been paused/cleared/completed.
    void retryPendingRoute();
    void clearPendingRoute();
    void armRerouteRetry();
    void resetBlockedRoad();
    void reportBlockedRoadError(const QString &message);

    // --- Plan and hop state ---
    void setPlanState(RoutePlanState state);
    void restorePlan();
    void requestPlan(const QString &method, const QJsonObject &payload,
                     std::function<void()> accepted = {});
    void applyPlanSnapshot(const QJsonObject &snapshot);
    QJsonObject progressArgs() const;
    void clearLocalNavigation();
    void showHopReached();
    // Teardown of the previous hop, then target the current stop and request a
    // route. Shared by plan creation, hop advance, and resume.
    void beginCurrentHop(ValhallaClient::Reason reason);
    void onHopReached();
    void advanceToNextHop();
    void completePlan();
    void startHopAdvanceTimer();
    void stopHopAdvanceTimer();
    // Ask for a fresh per-hop preview of the remaining plan (one multi-stop
    // request) and rebuild planOverview from the answer.
    void refreshPlanOverview();
    void clearPlanOverview();

    // Local wall-clock "now" derived from GPS time, formatted for Valhalla's
    // date_time.value, or empty when no trusted GPS time is available. GPS time
    // is used (not the system clock) because the DBC RTC can be wrong at boot;
    // the monotonic fix-age correction makes it immune to RTC skew.
    QString gpsDepartureTimeLocal() const;

    // Oldest GPS time we'll trust for the route departure time. TPVs arrive at
    // ~1 Hz while navigating; beyond this the fix is stale enough that the
    // age-corrected time could have drifted, so we omit date_time instead.
    static constexpr qint64 GpsTimeMaxAgeMs = 300000;  // 5 min

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

    // Post-transition confirmation window: show the just-passed maneuver's
    // verbal_post ("Continue for 300 m on Oak") for a brief moment after the
    // rider crosses the maneuver, provided the next turn is far enough away
    // that the confirmation doesn't crowd out its alert text.
    static constexpr int    PostWindowMs        = 6000;
    static constexpr double PostMinUpcomingGap  = 400.0;  // next must be > this to show post

    GpsStore *m_gps;
    NavigationStore *m_nav;
    VehicleStore *m_vehicle;
    SettingsStore *m_settings;
    SpeedLimitStore *m_speedLimit;
    ValhallaClient *m_valhalla;
    MapService *m_map = nullptr;

    // Exact integer division of the 20 Hz estimator tick avoids elapsed-time
    // jitter turning a nominal 5 Hz update into an alternating 4/5 Hz beat.
    NavigationCadence::TickDivider m_navigationCadence{
        NavigationCadence::NavigationEveryTicks};
    // Elapsed since the current route was calculated. Used as a safety
    // valve to drop kStart-family maneuvers if the segment tracker hasn't
    // advanced past segment 0 for a while (rider stationary after setting
    // destination, or segment 0 is longer than typical).
    QElapsedTimer m_routeStartedAt;
    // How long the kStart banner lingers before being dropped if the rider
    // hasn't advanced past segment 0 yet. Matches "a few seconds" — long
    // enough to read the initial heading, short enough to get out of the
    // way when the first real maneuver arrives.
    static constexpr int StartMaxLingerMs = 8000;

    NavigationStatus m_status = NavigationStatus::Idle;
    Route m_route;
    LatLng m_destination;
    LatLng m_blockedLocation;
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

    // Post-transition confirmation: the last maneuver we were tracking,
    // captured at the moment the rider crossed its shape index. Exposed
    // to currentVerbalInstruction() as a short-lived override so we can
    // surface Valhalla's "Continue for X m on Oak" line after the turn.
    RouteInstruction m_lastPassedManeuver;
    bool m_hasLastPassedManeuver = false;
    QElapsedTimer m_lastPassedAt;
    int m_prevLeadingShapeIdx = -1;

    // Roundabout icon payload, cached on the shape indices of the Enter/Exit
    // pair it was built from.
    QVariantMap m_roundaboutRender;
    int m_roundaboutEnterShape = -1;
    int m_roundaboutExitShape = -1;

    // A deviation is allowed one queued reroute until it is either dispatched
    // or explicitly retried after the client's cooldown. This keeps the 5 Hz
    // navigation update from continually replacing the pending request.
    RerouteEpisodeGate m_rerouteGate;
    QTimer *m_rerouteRetry = nullptr;
    ValhallaClient::Reason m_activeRouteReason = ValhallaClient::Reason::Initial;

    // A user-visible route request that is waiting for a usable origin. Reason
    // is kept so the retry re-issues the same kind of request; pendingHopId
    // lets a plan change supersede it. Only one request is ever held.
    bool m_pendingRoute = false;
    ValhallaClient::Reason m_pendingRouteReason = ValhallaClient::Reason::Initial;
    int m_pendingRouteHopId = -1;

    // How long the error pill stays up before it drops itself. Matches
    // ToastService's error duration so the pill and its toast go together.
    static constexpr int ErrorLingerMs = 5000;
    QTimer *m_errorLinger = nullptr;

    // Seconds the drive-mode arrival notice shows before advancing. Opening
    // the seatbox Shortcut menu suspends the countdown while choosing Keep.
    static constexpr int HopAdvanceTimeoutSeconds = 30;

    RoutePlanRpc m_planRpc;
    QString m_planId;
    QStringList m_stopIds;
    QHash<QString, int> m_numericStopIds;
    int m_nextStopId = 1;
    quint64 m_planRevision = 0;
    bool m_progressPending = false;
    bool m_restorePending = true;
    int m_pendingPlanMutations = 0;
    RoutePlan m_plan;
    RoutePlanState m_planState = RoutePlanState::None;
    // Repeating 1 Hz timer that also drives the countdown property; fires
    // advanceToNextHop() when it reaches zero.
    QTimer *m_hopAdvance = nullptr;
    int m_hopSecondsLeft = 0;
    bool m_hopMenuOpen = false;
    // True when dismount finished the current hop. Unlock advances to the next.
    bool m_pausedAfterReach = false;
    bool m_restoreReachedAwaitingVehicleState = false;

    QVariantList m_planOverview;
    double m_planTotalDistance = 0;
    double m_planTotalDuration = 0;
    QList<LatLng> m_planGeometry;
};

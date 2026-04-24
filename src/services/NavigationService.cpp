#include "NavigationService.h"
#include "MapService.h"
#include "routing/ValhallaClient.h"
#include "routing/RouteHelpers.h"
#include "stores/GpsStore.h"
#include "stores/NavigationStore.h"
#include "stores/VehicleStore.h"
#include "stores/SettingsStore.h"
#include "stores/SpeedLimitStore.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QDebug>
#include <QDateTime>
#include <QPointF>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <cmath>

namespace {
// Stock Valhalla uses the posted OSM maxspeed verbatim as the routing
// speed, which is wildly optimistic in dense urban areas. The tile build
// pipeline in librescoot/valhalla-tiles was switched on 2026-04-24 to
// post-process tiles with valhalla_assign_speeds + the OpenStreetMapSpeeds
// default_speeds.json, producing realistic edge speeds that no longer
// need client-side compensation.
//
// Tiles built *before* that cutoff still overreport. We pad those, but
// only when we're talking to a local Valhalla — a remote endpoint
// (FOSSGIS, self-hosted) is assumed to already have realistic speeds.
//
// TODO: remove this pad and kPadCutoffUtc around 2026-05-24 once all
// deployed scooters have picked up tiles built after the cutoff.
constexpr double kDurationPadFactor = 1.20;
const QDateTime kPadCutoffUtc =
    QDateTime(QDate(2026, 4, 24), QTime(0, 0), Qt::UTC);
}

NavigationService::NavigationService(GpsStore *gps, NavigationStore *nav,
                                       VehicleStore *vehicle, SettingsStore *settings,
                                       SpeedLimitStore *speedLimit, MdbRepository *repo,
                                       QObject *parent)
    : QObject(parent)
    , m_gps(gps)
    , m_nav(nav)
    , m_vehicle(vehicle)
    , m_settings(settings)
    , m_speedLimit(speedLimit)
    , m_repo(repo)
{
    m_valhalla = new ValhallaClient(this);

    // Set endpoint and language from settings
    QString url = settings->valhallaUrl();
    if (!url.isEmpty()) {
        m_valhalla->setEndpoint(url);
    }
    m_valhalla->setLanguage(settings->language());

    // Connect Valhalla signals
    connect(m_valhalla, &ValhallaClient::routeCalculated,
            this, &NavigationService::onRouteCalculated);
    connect(m_valhalla, &ValhallaClient::routeError,
            this, &NavigationService::onRouteError);
    connect(m_valhalla, &ValhallaClient::requestRejected,
            this, &NavigationService::onRequestRejected);

    // Listen to GPS updates
    connect(gps, &GpsStore::latitudeChanged, this, &NavigationService::onGpsChanged);
    connect(gps, &GpsStore::longitudeChanged, this, &NavigationService::onGpsChanged);

    // Listen to navigation store (destination set externally via Redis)
    // Debounce: lat/lng/destination signals may fire individually from doHgetall,
    // so coalesce into a single handler call to avoid partial-data route requests
    m_navDataDebounce = new QTimer(this);
    m_navDataDebounce->setSingleShot(true);
    m_navDataDebounce->setInterval(100);
    connect(m_navDataDebounce, &QTimer::timeout, this, &NavigationService::onNavigationDataChanged);

    auto debounceNav = [this]() { m_navDataDebounce->start(); };
    connect(nav, &NavigationStore::latitudeChanged, this, debounceNav);
    connect(nav, &NavigationStore::longitudeChanged, this, debounceNav);
    connect(nav, &NavigationStore::destinationChanged, this, debounceNav);

    // Listen to vehicle state (for shutdown-based navigation clearing)
    connect(vehicle, &VehicleStore::stateChanged,
            this, &NavigationService::onVehicleStateChanged);

    // Listen to Valhalla URL changes
    connect(settings, &SettingsStore::valhallaUrlChanged, this, [this]() {
        QString url = m_settings->valhallaUrl();
        if (!url.isEmpty()) {
            m_valhalla->setEndpoint(url);
        }
    });

    // Listen to language changes — recalculate route to get translated directions
    connect(settings, &SettingsStore::languageChanged, this, [this]() {
        m_valhalla->setLanguage(m_settings->language());
        if (isNavigating() && hasValidGps() && m_destination.isValid()) {
            m_valhalla->requestRoute(currentGpsPosition(), m_destination,
                                     ValhallaClient::Reason::LanguageChange);
        }
    });

    m_lastDrUpdate.start();
}

void NavigationService::setMapService(MapService *map)
{
    if (m_map == map) return;
    m_map = map;
    if (m_map) {
        connect(m_map, &MapService::vehiclePositionChanged,
                this, &NavigationService::onVehiclePositionChanged);
        // Segment updates arrive from MapService's GPS-edge matcher AND
        // from DR-tick advancement. Both route through routeProjectionChanged;
        // re-use the same throttled update path so TBT reflects matcher
        // decisions promptly (instead of waiting up to 200 ms for the next
        // DR tick to elapse).
        connect(m_map, &MapService::routeProjectionChanged,
                this, &NavigationService::onVehiclePositionChanged);
    }
}

void NavigationService::onVehiclePositionChanged()
{
    // Drive TBT from DR. Throttled to DrUpdateMinIntervalMs so route
    // snapping + upcoming-instruction walks don't run at the full 15 Hz
    // tick rate.
    if (m_status != NavigationStatus::Navigating &&
        m_status != NavigationStatus::Rerouting &&
        m_status != NavigationStatus::Arrived)
        return;

    if (m_lastDrUpdate.isValid() &&
        m_lastDrUpdate.elapsed() < DrUpdateMinIntervalMs)
        return;

    m_lastDrUpdate.restart();
    updateNavigationState();
}

// --- Property getters for current instruction ---

int NavigationService::currentManeuverType() const
{
    if (m_upcomingInstructions.isEmpty())
        return static_cast<int>(ManeuverType::Other);
    return static_cast<int>(m_upcomingInstructions.first().type);
}

double NavigationService::currentManeuverDistance() const
{
    if (m_upcomingInstructions.isEmpty()) return 0;
    return m_upcomingInstructions.first().distance;
}

QString NavigationService::currentStreetName() const
{
    if (m_upcomingInstructions.isEmpty()) return {};
    return m_upcomingInstructions.first().streetName;
}

QString NavigationService::currentVerbalInstruction() const
{
    if (m_upcomingInstructions.isEmpty()) return {};
    const auto &instr = m_upcomingInstructions.first();

    // Stage is advanced by updateVerbalStage(); read it here.
    switch (m_currentVerbalStage) {
    case 0: return instr.verbalAlertInstruction;
    case 1: return instr.verbalPreTransitionInstruction;
    default:
        return instr.verbalSuccinctInstruction.isEmpty()
                   ? instr.instructionText
                   : instr.verbalSuccinctInstruction;
    }
}

QString NavigationService::currentInstructionText() const
{
    if (m_upcomingInstructions.isEmpty()) return {};
    return m_upcomingInstructions.first().instructionText;
}

int NavigationService::roundaboutExitCount() const
{
    if (m_upcomingInstructions.isEmpty()) return 0;
    return m_upcomingInstructions.first().roundaboutExitCount;
}

QVariantMap NavigationService::currentRoundaboutRender() const
{
    QVariantMap result;
    if (m_upcomingInstructions.isEmpty())
        return result;

    const RouteInstruction &current = m_upcomingInstructions.first();
    if (current.type != ManeuverType::RoundaboutEnter &&
        current.type != ManeuverType::RoundaboutExit)
        return result;

    // Find the Enter/Exit pair in m_route.instructions so the same icon works
    // while approaching (current=Enter) and while inside the ring (current=Exit).
    int enterInstrIdx = -1;
    int exitInstrIdx = -1;
    for (int i = 0; i < m_route.instructions.size(); ++i) {
        if (m_route.instructions[i].originalShapeIndex == current.originalShapeIndex &&
            m_route.instructions[i].type == current.type) {
            if (current.type == ManeuverType::RoundaboutEnter) {
                enterInstrIdx = i;
                for (int j = i + 1; j < m_route.instructions.size(); ++j) {
                    if (m_route.instructions[j].type == ManeuverType::RoundaboutExit) {
                        exitInstrIdx = j;
                        break;
                    }
                }
            } else {
                exitInstrIdx = i;
                for (int j = i - 1; j >= 0; --j) {
                    if (m_route.instructions[j].type == ManeuverType::RoundaboutEnter) {
                        enterInstrIdx = j;
                        break;
                    }
                }
            }
            break;
        }
    }
    if (enterInstrIdx < 0 || exitInstrIdx < 0)
        return result;

    const int enterIdx = m_route.instructions[enterInstrIdx].originalShapeIndex;
    const int exitIdx = m_route.instructions[exitInstrIdx].originalShapeIndex;

    // Clamp and sanity.
    if (enterIdx < 0 || enterIdx >= m_route.waypoints.size())
        return result;
    if (exitIdx <= enterIdx || exitIdx >= m_route.waypoints.size())
        return result;

    // Arc points on the ring itself — used for the circle fit.
    QList<LatLng> arcPoints;
    arcPoints.reserve(exitIdx - enterIdx + 1);
    for (int i = enterIdx; i <= exitIdx; ++i)
        arcPoints.append(m_route.waypoints[i]);

    if (arcPoints.size() < 2)
        return result;

    // Display path extends ~8 waypoints either side of the arc so the icon
    // shows the approach stub coming in and the exit stub going out in white,
    // not just the on-ring arc.
    constexpr int kStubPoints = 8;
    const int pathStart = std::max(0, enterIdx - kStubPoints);
    const int pathEnd = std::min(static_cast<int>(m_route.waypoints.size()) - 1,
                                   exitIdx + kStubPoints);
    QList<LatLng> pathPoints;
    pathPoints.reserve(pathEnd - pathStart + 1);
    for (int i = pathStart; i <= pathEnd; ++i)
        pathPoints.append(m_route.waypoints[i]);

    // Fit a circle through three on-ring arc points (first, middle, last) in a
    // local east/north metric frame — gives the ring centroid instead of the
    // arc midpoint so the icon actually frames the roundabout. Mean fallback
    // if the three points are collinear or if we have fewer than three points.
    double centerLat, centerLon;
    auto meanFallback = [&](double &lat, double &lon) {
        double latSum = 0, lonSum = 0;
        for (const auto &p : arcPoints) { latSum += p.latitude; lonSum += p.longitude; }
        lat = latSum / arcPoints.size();
        lon = lonSum / arcPoints.size();
    };
    if (arcPoints.size() >= 3) {
        const LatLng &A = arcPoints.first();
        const LatLng &B = arcPoints[arcPoints.size() / 2];
        const LatLng &C = arcPoints.last();
        const double cosLat0 = std::cos(A.latitude * M_PI / 180.0);
        const auto toEN = [&](const LatLng &p) {
            return QPointF((p.longitude - A.longitude) * 111320.0 * cosLat0,
                           (p.latitude - A.latitude) * 111320.0);
        };
        const QPointF a = toEN(A), b = toEN(B), c = toEN(C);
        const double d = 2.0 * (a.x() * (b.y() - c.y()) +
                                b.x() * (c.y() - a.y()) +
                                c.x() * (a.y() - b.y()));
        if (std::abs(d) > 1e-6) {
            const double aSq = a.x() * a.x() + a.y() * a.y();
            const double bSq = b.x() * b.x() + b.y() * b.y();
            const double cSq = c.x() * c.x() + c.y() * c.y();
            const double ux = (aSq * (b.y() - c.y()) + bSq * (c.y() - a.y()) + cSq * (a.y() - b.y())) / d;
            const double uy = (aSq * (c.x() - b.x()) + bSq * (a.x() - c.x()) + cSq * (b.x() - a.x())) / d;
            centerLat = A.latitude + uy / 111320.0;
            centerLon = A.longitude + ux / (111320.0 * cosLat0);
        } else {
            meanFallback(centerLat, centerLon);
        }
    } else {
        meanFallback(centerLat, centerLon);
    }

    QVariantList path;
    path.reserve(pathPoints.size());
    for (const auto &pt : pathPoints) {
        QVariantList p;
        p << pt.latitude << pt.longitude;
        path.append(QVariant(p));
    }

    result[QStringLiteral("centerLat")] = centerLat;
    result[QStringLiteral("centerLon")] = centerLon;
    // Always orient by the approach bearing so the icon stays stable from the
    // Enter maneuver through the Exit maneuver (we'd otherwise rotate as the
    // scooter tracks around the ring).
    result[QStringLiteral("bearingDeg")] = m_route.instructions[enterInstrIdx].bearingBefore;
    result[QStringLiteral("path")] = path;
    return result;
}

// --- Next instruction (preview) ---

int NavigationService::nextManeuverType() const
{
    if (m_upcomingInstructions.size() < 2)
        return static_cast<int>(ManeuverType::Other);
    return static_cast<int>(m_upcomingInstructions[1].type);
}

double NavigationService::nextManeuverDistance() const
{
    if (m_upcomingInstructions.size() < 2) return 0;
    return m_upcomingInstructions[1].distance;
}

QString NavigationService::nextStreetName() const
{
    if (m_upcomingInstructions.size() < 2) return {};
    return m_upcomingInstructions[1].streetName;
}

bool NavigationService::hasNextInstruction() const
{
    return m_upcomingInstructions.size() >= 2;
}

bool NavigationService::showNextPreview() const
{
    if (m_upcomingInstructions.size() < 2) return false;
    // State flipped by updateNextPreviewState(); multi-cue suppresses.
    return m_nextPreviewShown && !m_upcomingInstructions.first().verbalMultiCue;
}

double NavigationService::remainingDuration() const
{
    return m_remainingDuration * durationPadFactor();
}

QString NavigationService::eta() const
{
    if (!m_route.isValid() || m_remainingDuration <= 0) return {};
    QDateTime arrival = QDateTime::currentDateTime().addSecs(
        static_cast<qint64>(m_remainingDuration * durationPadFactor()));
    return arrival.toString(QStringLiteral("HH:mm"));
}

double NavigationService::durationPadFactor() const
{
    if (!m_valhalla)
        return kDurationPadFactor;

    const bool local =
        QUrl(m_valhalla->endpoint()).host() == QStringLiteral("127.0.0.1");
    if (!local)
        return 1.0;

    const QDateTime ts = m_valhalla->tilesetLastModified();
    if (!ts.isValid())
        return kDurationPadFactor; // default to pad when unknown
    return ts < kPadCutoffUtc ? kDurationPadFactor : 1.0;
}

// --- Actions ---

void NavigationService::setDestination(double lat, double lng, const QString &address)
{
    qDebug() << "NavigationService::setDestination:" << lat << lng << address;

    if (lat == 0 && lng == 0) {
        qWarning() << "NavigationService::setDestination: ignoring (0,0) destination";
        m_errorMessage = QStringLiteral("Invalid destination coordinates");
        emit errorChanged();
        return;
    }

    m_destination = {lat, lng};
    m_destAddress = address;
    emit destinationChanged();

    // Write to Redis for other services
    QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("latitude"),
                QString::number(lat, 'f', 6));
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("longitude"),
                QString::number(lng, 'f', 6));
    if (!address.isEmpty()) {
        m_repo->set(QStringLiteral("navigation"), QStringLiteral("address"), address);
    }
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("timestamp"), timestamp);
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("destination"),
                QString::number(lat, 'f', 6) + QLatin1Char(',') +
                QString::number(lng, 'f', 6));
    m_repo->publish(QStringLiteral("navigation"), QStringLiteral("updated"));

    if (!hasValidGps()) {
        qDebug() << "NavigationService: waiting for GPS fix before calculating route";
        return;
    }
    LatLng from = currentGpsPosition();
    if (!from.isValid()) {
        qDebug() << "NavigationService: GPS position is (0,0)";
        return;
    }
    setStatus(NavigationStatus::Calculating);
    m_valhalla->requestRoute(from, m_destination, ValhallaClient::Reason::Destination);
}

void NavigationService::clearNavigation()
{
    m_valhalla->cancelPending();
    m_route = Route();
    m_destination = {};
    m_destAddress.clear();
    m_upcomingInstructions.clear();
    m_distanceToDestination = 0;
    m_remainingDuration = 0;
    m_distanceFromRoute = 0;
    m_isOffRoute = false;
    m_wasArrived = false;
    m_currentSegmentIndex = 0;

    setStatus(NavigationStatus::Idle);
    emit routeChanged();
    emit destinationChanged();
    emit instructionChanged();
    emit positionChanged();

    // Clear Redis
    m_repo->hdel(QStringLiteral("navigation"), QStringLiteral("latitude"));
    m_repo->hdel(QStringLiteral("navigation"), QStringLiteral("longitude"));
    m_repo->hdel(QStringLiteral("navigation"), QStringLiteral("address"));
    m_repo->hdel(QStringLiteral("navigation"), QStringLiteral("timestamp"));
    m_repo->hdel(QStringLiteral("navigation"), QStringLiteral("destination"));
    m_repo->publish(QStringLiteral("navigation"), QStringLiteral("cleared"));
}

void NavigationService::setRoute(const Route &route)
{
    onRouteCalculated(route);
}

// --- Slot handlers ---

void NavigationService::onGpsChanged()
{
    // Update position when GPS fix is recent; keep last known position when stale
    // so navigation and dead reckoning can continue through brief GPS gaps
    if (!m_gps) return;

    bool hasPosition = (m_gps->latitude() != 0 || m_gps->longitude() != 0);
    if (!hasPosition) return;

    // Nav state is normally driven by MapService's DR tick via
    // onVehiclePositionChanged. Only run here as a fallback when DR isn't
    // initialised or is paused (e.g. simulator freeze), so boot + frozen
    // states still react to GPS edges.
    bool drReady = m_map && m_map->hasVehiclePosition();
    if (!drReady &&
        (m_status == NavigationStatus::Navigating ||
         m_status == NavigationStatus::Rerouting ||
         m_status == NavigationStatus::Arrived)) {
        m_lastDrUpdate.restart();
        updateNavigationState();
    }

    // Recovery: destination loaded but route not yet calculated (GPS wasn't ready)
    if (m_destination.isValid() && !m_route.isValid() &&
        (m_status == NavigationStatus::Idle || m_status == NavigationStatus::Error)) {
        if (hasValidGps()) {
            LatLng pos = currentPosition();
            double dist = pos.distanceTo(m_destination);
            if (dist < ArrivalProximity) {
                clearNavigation();
            } else {
                LatLng from = currentGpsPosition();
                if (from.isValid()) {
                    m_valhalla->requestRoute(from, m_destination,
                                             ValhallaClient::Reason::Recovery);
                }
            }
        }
    }
}

void NavigationService::onNavigationDataChanged()
{
    if (!m_nav) return;

    // Parse destination from Redis store fields
    double lat = m_nav->latitude().toDouble();
    double lng = m_nav->longitude().toDouble();

    // Fallback to legacy "lat,lon" destination field
    if (lat == 0 && lng == 0 && !m_nav->destination().isEmpty()) {
        QStringList parts = m_nav->destination().split(QLatin1Char(','));
        if (parts.size() == 2) {
            lat = parts[0].toDouble();
            lng = parts[1].toDouble();
        }
    }

    if (lat == 0 && lng == 0) {
        // Destination cleared externally
        if (m_destination.isValid()) {
            clearNavigation();
        }
        return;
    }

    LatLng newDest{lat, lng};
    // Don't recalculate if same destination and we are already Navigating or Calculating
    if (newDest == m_destination && (m_status == NavigationStatus::Navigating || m_status == NavigationStatus::Calculating)) {
        return; 
    }

    m_destination = newDest;
    m_destAddress = m_nav->address();
    emit destinationChanged();

    if (hasValidGps()) {
        LatLng from = currentGpsPosition();
        if (from.isValid()) {
            setStatus(NavigationStatus::Calculating);
            m_valhalla->requestRoute(from, m_destination,
                                     ValhallaClient::Reason::Destination);
        }
    } else {
        qDebug() << "NavigationService: waiting for GPS fix before calculating route";
    }
}

void NavigationService::onVehicleStateChanged()
{
    if (!m_vehicle) return;

    // Clear navigation if shutting down or entering stand-by near destination
    bool isLocking = m_vehicle->isShuttingDown() || m_vehicle->isStandBy();
    if (isLocking && m_destination.isValid()) {
        LatLng pos = currentPosition();
        double dist = pos.distanceTo(m_destination);
        if (dist < ShutdownProximity || m_wasArrived) {
            qDebug() << "NavigationService: clearing navigation (lock near destination)";
            clearNavigation();
        }
    }
}

void NavigationService::onRouteCalculated(const Route &route)
{
    m_route = route;
    m_remainingDuration = route.duration;
    m_currentSegmentIndex = 0;
    m_wasArrived = false;

    if (!m_destination.isValid() && !route.waypoints.isEmpty()) {
        m_destination = route.waypoints.last();
        emit destinationChanged();
    }

    setStatus(NavigationStatus::Navigating);
    emit routeChanged();

    // Immediately update with current GPS position
    if (hasValidGps()) {
        updateNavigationState();
    }

    qDebug() << "NavigationService: route calculated -"
             << route.waypoints.size() << "waypoints,"
             << route.instructions.size() << "instructions,"
             << (route.distance / 1000.0) << "km";
}

void NavigationService::onRouteError(const QString &error)
{
    m_errorMessage = error;
    setStatus(NavigationStatus::Error);
    emit errorChanged();
    qWarning() << "NavigationService: route error -" << error;
}

void NavigationService::onRequestRejected(ValhallaClient::Reason reason,
                                           ValhallaClient::RejectionCause cause)
{
    const bool userReason =
        reason == ValhallaClient::Reason::Initial ||
        reason == ValhallaClient::Reason::Destination ||
        reason == ValhallaClient::Reason::LanguageChange;

    if (userReason) {
        if (cause == ValhallaClient::RejectionCause::RateLimited) {
            m_errorMessage = QStringLiteral("Too many routing requests");
            setStatus(NavigationStatus::Error);
            emit errorChanged();
            return;
        }
        if (cause == ValhallaClient::RejectionCause::Unhealthy) {
            m_errorMessage = QStringLiteral("Cannot reach routing server");
            setStatus(NavigationStatus::Error);
            emit errorChanged();
            return;
        }
    }

    // Auto rejections (Reroute/Recovery) silently drop; the next trigger
    // (GPS edge, DR tick) will retry when the gate reopens.
    qDebug() << "NavigationService: request rejected reason=" << static_cast<int>(reason)
             << "cause=" << static_cast<int>(cause);
}

// --- Internal ---

void NavigationService::updateNavigationState()
{
    if (!m_route.isValid()) return;

    LatLng pos = currentPosition();
    if (!pos.isValid()) return;

    // Distance to destination
    m_distanceToDestination = pos.distanceTo(m_destination);

    // Route projection is authoritative on MapService (trajectory-aware
    // matcher). Prefer its values so TBT, off-route detection, and the
    // upcoming-instruction walker all share one source of truth. Fall back
    // to local global-nearest only when MapService state isn't available
    // (startup, no route yet, etc).
    int segIdx;
    double distFromRoute;
    if (m_map && m_map->hasVehiclePosition() &&
        m_map->currentRouteSegment() >= 0) {
        segIdx = m_map->currentRouteSegment();
        distFromRoute = m_map->distanceFromRoute();
        m_snappedPosition = {m_map->segmentSnappedLatitude(), m_map->segmentSnappedLongitude()};
    } else {
        auto [snapped, idx, dist] =
            RouteHelpers::findClosestPointOnRoute(pos, m_route.waypoints);
        segIdx = idx;
        distFromRoute = dist;
        m_snappedPosition = snapped;
    }
    m_distanceFromRoute = distFromRoute;
    m_currentSegmentIndex = segIdx;

    // Arrival detection
    if (m_distanceToDestination < ArrivalProximity) {
        if (m_status != NavigationStatus::Arrived) {
            m_wasArrived = true;
            setStatus(NavigationStatus::Arrived);
        }
        emit positionChanged();
        return;
    }

    // Departure from arrival zone
    if (m_wasArrived && m_status == NavigationStatus::Arrived) {
        if (!m_vehicle->isShuttingDown()) {
            m_wasArrived = false;
            setStatus(NavigationStatus::Navigating);
        }
    }

    // Off-route detection with hysteresis to prevent boundary oscillation
    if (m_isOffRoute) {
        m_isOffRoute = distFromRoute > OnRouteTolerance;
    } else {
        m_isOffRoute = distFromRoute > OffRouteTolerance;
    }

    // Reroute while off-route — ValhallaClient enforces cooldown/backoff.
    // Status flips to Rerouting regardless of accept; if the client rejects,
    // onRequestRejected handles the error path.
    if (m_isOffRoute) {
        LatLng from = currentGpsPosition();
        if (from.isValid() && m_destination.isValid()) {
            m_valhalla->requestRoute(from, m_destination,
                                     ValhallaClient::Reason::Reroute);
            setStatus(NavigationStatus::Rerouting);
        }
    } else if (!m_isOffRoute && m_status == NavigationStatus::Error) {
        setStatus(NavigationStatus::Navigating);
    }

    // Find upcoming instructions. Pass the snapped position (not raw pos) so
    // along-route distance starts from the projection onto the current
    // segment. For the fallback path where MapService state isn't available,
    // m_snappedPosition was set to the local global-nearest projection above.
    auto upcoming = RouteHelpers::findUpcomingInstructions(
        m_snappedPosition, m_route, m_currentSegmentIndex, 3);

    if (upcoming != m_upcomingInstructions) {
        m_upcomingInstructions = upcoming;
        emit instructionChanged();
    }

    // Update verbal-stage and next-preview hysteresis state before any
    // property reader observes the instruction change.
    if (!m_upcomingInstructions.isEmpty())
        updateVerbalStage(m_upcomingInstructions.first());
    updateNextPreviewState();

    // Remaining duration = (time to reach the next maneuver at the CURRENT
    // segment's speed) + (full durations of that maneuver and all after).
    // The current-segment speed is the one for the step we're traversing NOW
    // (the maneuver at firstIdx-1 if present, else the whole-route average).
    // Prior versions used the upcoming maneuver's own speed for the approach
    // leg — wrong leg, can skew ETA noticeably at road-class transitions.
    // kDurationPadFactor still applies on read for the optimism correction.
    if (!upcoming.isEmpty()) {
        int firstIdx = -1;
        for (int i = 0; i < m_route.instructions.size(); ++i) {
            if (m_route.instructions[i].originalShapeIndex ==
                upcoming.first().originalShapeIndex) {
                firstIdx = i;
                break;
            }
        }
        if (firstIdx >= 0) {
            double remaining = 0;

            // Full duration of the upcoming maneuver + all that follow
            for (int i = firstIdx; i < m_route.instructions.size(); ++i)
                remaining += m_route.instructions[i].duration;

            // Time to reach the upcoming maneuver from current position,
            // using the speed of the step we're currently on.
            double sPerM = 0;
            if (firstIdx > 0) {
                const auto &cur = m_route.instructions[firstIdx - 1];
                if (cur.distance > 0)
                    sPerM = cur.duration / cur.distance;
            }
            if (sPerM <= 0 && m_route.distance > 0) {
                sPerM = m_route.duration / m_route.distance;
            }
            remaining += upcoming.first().distance * sPerM;

            m_remainingDuration = remaining;
        }
    }

    emit positionChanged();
}

void NavigationService::setStatus(NavigationStatus status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void NavigationService::updateVerbalStage(const RouteInstruction &first)
{
    // Reset on maneuver change: seed the stage from the current distance
    // against hard thresholds so we don't start in the wrong bucket.
    if (first.originalShapeIndex != m_currentVerbalInstrShapeIdx) {
        m_currentVerbalInstrShapeIdx = first.originalShapeIndex;
        if (first.distance > 300.0)       m_currentVerbalStage = 0;
        else if (first.distance > 50.0)   m_currentVerbalStage = 1;
        else                              m_currentVerbalStage = 2;
        return;
    }

    const double d = first.distance;
    switch (m_currentVerbalStage) {
    case 0: // alert
        if (d <= VerbalAlertExit) m_currentVerbalStage = 1;
        break;
    case 1: // pre-transition
        if (d <= VerbalSuccinctEnter)       m_currentVerbalStage = 2;
        else if (d >= VerbalAlertEnter)     m_currentVerbalStage = 0;
        break;
    case 2: // succinct
        if (d >= VerbalSuccinctExit) m_currentVerbalStage = 1;
        break;
    }
}

void NavigationService::updateNextPreviewState()
{
    if (m_upcomingInstructions.size() < 2) {
        m_nextPreviewShown = false;
        m_nextPreviewInstrShapeIdx = -1;
        return;
    }
    const auto &next = m_upcomingInstructions[1];
    if (next.originalShapeIndex != m_nextPreviewInstrShapeIdx) {
        m_nextPreviewInstrShapeIdx = next.originalShapeIndex;
        m_nextPreviewShown = next.distance < 300.0;
        return;
    }
    if (m_nextPreviewShown) {
        if (next.distance >= NextPreviewHide) m_nextPreviewShown = false;
    } else {
        if (next.distance <= NextPreviewShow) m_nextPreviewShown = true;
    }
}

LatLng NavigationService::currentGpsPosition() const
{
    if (!m_gps) return {};
    return {m_gps->latitude(), m_gps->longitude()};
}

LatLng NavigationService::currentPosition() const
{
    // Prefer the dead-reckoned position from MapService so nav stays in sync
    // with the vehicle marker between GPS samples. Fall back to raw GPS when
    // DR isn't initialised yet.
    if (m_map && m_map->hasVehiclePosition()) {
        double lat = m_map->vehicleLatitude();
        double lng = m_map->vehicleLongitude();
        if (lat != 0 || lng != 0) return {lat, lng};
    }
    return currentGpsPosition();
}

bool NavigationService::hasValidGps() const
{
    if (!m_gps) return false;
    // Accept any non-zero position — don't require FixEstablished or recent
    // timestamp, as the GPS daemon may be slow to update state even though
    // we already have usable coordinates (visible on the map).
    return m_gps->latitude() != 0 || m_gps->longitude() != 0;
}

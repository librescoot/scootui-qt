#include "NavigationService.h"
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

    // Set endpoint from settings or default
    QString url = settings->valhallaUrl();
    if (!url.isEmpty()) {
        m_valhalla->setEndpoint(url);
    }

    // Connect Valhalla signals
    connect(m_valhalla, &ValhallaClient::routeCalculated,
            this, &NavigationService::onRouteCalculated);
    connect(m_valhalla, &ValhallaClient::routeError,
            this, &NavigationService::onRouteError);
    connect(m_valhalla, &ValhallaClient::rateLimited, this, [this]() {
        // Back off for 60s on 429 to avoid hammering the server
        m_lastRerouteTime.restart();
        m_rateLimitBackoffMs = qMin(qMax(m_rateLimitBackoffMs * 2, 30000), 120000);
        qDebug() << "Rate limited, backing off for" << m_rateLimitBackoffMs << "ms";
    });

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

    m_lastRerouteTime.start();
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

    // Choose verbal instruction based on distance
    if (instr.distance > 300)
        return instr.verbalAlertInstruction;
    if (instr.distance > 50)
        return instr.verbalPreTransitionInstruction;
    return instr.verbalSuccinctInstruction.isEmpty()
               ? instr.instructionText
               : instr.verbalSuccinctInstruction;
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
    const auto &next = m_upcomingInstructions[1];
    // Show preview if next maneuver is within 300m and not multi-cue
    return next.distance < 300 && !m_upcomingInstructions.first().verbalMultiCue;
}

QString NavigationService::eta() const
{
    if (!m_route.isValid() || m_remainingDuration <= 0) return {};
    QDateTime arrival = QDateTime::currentDateTime().addSecs(
        static_cast<qint64>(m_remainingDuration));
    return arrival.toString(QStringLiteral("HH:mm"));
}

// --- Actions ---

void NavigationService::setDestination(double lat, double lng, const QString &address)
{
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

    calculateRoute();
}

void NavigationService::clearNavigation()
{
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

    if (m_status == NavigationStatus::Navigating ||
        m_status == NavigationStatus::Rerouting ||
        m_status == NavigationStatus::Arrived) {
        updateNavigationState();
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
        calculateRoute();
    } else {
        qDebug() << "NavigationService: waiting for GPS fix before calculating route";
    }
}

void NavigationService::onVehicleStateChanged()
{
    if (!m_vehicle) return;

    // Clear navigation if shutting down near destination
    if (m_vehicle->isShuttingDown() && m_destination.isValid()) {
        LatLng pos = currentGpsPosition();
        double dist = pos.distanceTo(m_destination);
        if (dist < ShutdownProximity || m_wasArrived) {
            qDebug() << "NavigationService: clearing navigation (shutdown near destination)";
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

// --- Internal ---

void NavigationService::calculateRoute()
{
    if (!m_destination.isValid()) return;

    LatLng from = currentGpsPosition();
    if (!from.isValid()) {
        qDebug() << "NavigationService: no GPS position for route calculation";
        return;
    }

    setStatus(NavigationStatus::Calculating);
    m_valhalla->calculateRoute(from, m_destination);
}

void NavigationService::reroute()
{
    int cooldown = qMax(RerouteCooldownMs, m_rateLimitBackoffMs);
    if (m_lastRerouteTime.elapsed() < cooldown) return;
    m_lastRerouteTime.restart();
    m_rateLimitBackoffMs = 0;

    qDebug() << "NavigationService: rerouting (off-route)";
    setStatus(NavigationStatus::Rerouting);

    LatLng from = currentGpsPosition();
    m_valhalla->calculateRoute(from, m_destination);
}

void NavigationService::updateNavigationState()
{
    if (!m_route.isValid()) return;

    LatLng pos = currentGpsPosition();
    if (!pos.isValid()) return;

    // Distance to destination
    m_distanceToDestination = pos.distanceTo(m_destination);

    // Find position on route
    auto [snapped, segIdx, distFromRoute] =
        RouteHelpers::findClosestPointOnRoute(pos, m_route.waypoints);
    m_snappedPosition = snapped;
    m_currentSegmentIndex = segIdx;
    m_distanceFromRoute = distFromRoute;

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

    // Reroute while off-route (cooldown in reroute() prevents spam)
    if (m_isOffRoute && m_status != NavigationStatus::Rerouting) {
        reroute();
    } else if (!m_isOffRoute && m_status == NavigationStatus::Error) {
        setStatus(NavigationStatus::Navigating);
    }

    // Find upcoming instructions
    auto upcoming = RouteHelpers::findUpcomingInstructions(
        pos, m_route, m_currentSegmentIndex, 3);

    if (upcoming != m_upcomingInstructions) {
        m_upcomingInstructions = upcoming;
        emit instructionChanged();
    }

    // Calculate remaining duration from upcoming instructions (matching Flutter logic)
    if (!upcoming.isEmpty()) {
        // Find the index of the first upcoming instruction in the full route
        int firstIdx = -1;
        for (int i = 0; i < m_route.instructions.size(); ++i) {
            if (m_route.instructions[i].originalShapeIndex ==
                upcoming.first().originalShapeIndex) {
                firstIdx = i;
                break;
            }
        }
        if (firstIdx >= 0) {
            const auto &original = m_route.instructions[firstIdx];
            double remaining = 0;

            // Sum durations of all instructions after the first upcoming
            for (int i = firstIdx + 1; i < m_route.instructions.size(); ++i)
                remaining += m_route.instructions[i].duration;

            // Estimate time to reach the first upcoming maneuver proportionally
            if (original.distance > 0) {
                double speed = original.duration / original.distance; // s/m
                remaining += upcoming.first().distance * speed;
            }
            // Add the full duration of the first upcoming instruction's segment
            remaining += original.duration;

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

LatLng NavigationService::currentGpsPosition() const
{
    if (!m_gps) return {};
    return {m_gps->latitude(), m_gps->longitude()};
}

bool NavigationService::hasValidGps() const
{
    if (!m_gps) return false;
    return m_gps->hasRecentFix() &&
           (m_gps->latitude() != 0 || m_gps->longitude() != 0);
}

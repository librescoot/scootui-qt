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
#include <QTimeZone>
#include <QPointF>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
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
    QDateTime(QDate(2026, 4, 24), QTime(0, 0), QTimeZone::UTC);

// Compare plan stop lists by what the wire format carries (position and label).
// Ids and the reached flag are session state and are deliberately excluded, so
// our own write-back parses back equal and is ignored on ingest.
bool planStopsEqual(const QList<RouteStop> &a, const QList<RouteStop> &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).position != b.at(i).position)
            return false;
        if (a.at(i).label != b.at(i).label)
            return false;
    }
    return true;
}
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
    , m_planService(repo)
{
    m_valhalla = new ValhallaClient(this);
    m_valhalla->setDepartureTimeProvider([this]() { return gpsDepartureTimeLocal(); });

    // Set endpoint and language from settings
    QString url = settings->valhallaUrl();
    if (!url.isEmpty()) {
        m_valhalla->setEndpoint(url);
    }
    m_valhalla->setLanguage(settings->language());
    m_valhalla->setRoutePreference(settings->routePreference());
    m_valhalla->setAvoidCobblestone(settings->avoidCobblestone());

    // Connect Valhalla signals
    connect(m_valhalla, &ValhallaClient::routeCalculated,
            this, &NavigationService::onRouteCalculated);
    connect(m_valhalla, &ValhallaClient::routeAttributesReady,
            this, &NavigationService::onRouteAttributesReady);
    connect(m_valhalla, &ValhallaClient::routeError,
            this, &NavigationService::onRouteError);
    connect(m_valhalla, &ValhallaClient::requestRejected,
            this, &NavigationService::onRequestRejected);
    connect(m_valhalla, &ValhallaClient::requestDispatched,
            this, &NavigationService::onRequestDispatched);
    connect(m_valhalla, &ValhallaClient::planPreviewReady,
            this, &NavigationService::onPlanPreviewReady);
    connect(m_valhalla, &ValhallaClient::planPreviewFailed,
            this, &NavigationService::onPlanPreviewFailed);

    // Listen to GPS updates
    connect(gps, &GpsStore::sampleChanged, this, &NavigationService::onGpsChanged);

    // Listen to navigation store (destination set externally via Redis)
    // Debounce: lat/lng/destination signals may fire individually from doHgetall,
    // so coalesce into a single handler call to avoid partial-data route requests
    m_navDataDebounce = new QTimer(this);
    m_navDataDebounce->setSingleShot(true);
    m_navDataDebounce->setInterval(100);
    connect(m_navDataDebounce, &QTimer::timeout, this, &NavigationService::onNavigationDataChanged);

    m_errorLinger = new QTimer(this);
    m_errorLinger->setSingleShot(true);
    m_errorLinger->setInterval(ErrorLingerMs);
    connect(m_errorLinger, &QTimer::timeout, this, &NavigationService::clearError);

    m_rerouteRetry = new QTimer(this);
    m_rerouteRetry->setSingleShot(true);
    m_rerouteRetry->setInterval(ValhallaClient::RerouteCooldownMs);
    connect(m_rerouteRetry, &QTimer::timeout, this, [this]() {
        if (!m_isOffRoute || !m_route.isValid())
            return;
        m_rerouteGate.retryReady();
        updateNavigationState();
    });

    auto debounceNav = [this]() { m_navDataDebounce->start(); };
    connect(nav, &NavigationStore::latitudeChanged, this, debounceNav);
    connect(nav, &NavigationStore::longitudeChanged, this, debounceNav);
    connect(nav, &NavigationStore::destinationChanged, this, debounceNav);
    connect(nav, &NavigationStore::waypointsChanged, this, debounceNav);
    connect(nav, &NavigationStore::currentStepChanged, this, debounceNav);

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

    // Listen to routing preference changes — recalculate so the rider sees the
    // new preference applied to the trip they are on, not just the next one.
    auto applyRoutingPrefs = [this]() {
        m_valhalla->setRoutePreference(m_settings->routePreference());
        m_valhalla->setAvoidCobblestone(m_settings->avoidCobblestone());
        if (isNavigating() && m_destination.isValid())
            requestRoute(ValhallaClient::Reason::Destination);
        if (m_plan.isValid())
            refreshPlanOverview();
    };
    connect(settings, &SettingsStore::routePreferenceChanged, this, applyRoutingPrefs);
    connect(settings, &SettingsStore::avoidCobblestoneChanged, this, applyRoutingPrefs);

    // Listen to language changes — recalculate route to get translated directions
    connect(settings, &SettingsStore::languageChanged, this, [this]() {
        m_valhalla->setLanguage(m_settings->language());
        if (isNavigating() && m_destination.isValid())
            requestRoute(ValhallaClient::Reason::LanguageChange);
        if (m_plan.isValid())
            refreshPlanOverview();
    });

    // The continue prompt doubles as its own countdown display: one timer tick
    // per second, advancing automatically when it reaches zero. Parked/hop-on
    // pauses instead, so the rider never rolls into the next hop from a stop.
    m_hopAdvance = new QTimer(this);
    m_hopAdvance->setInterval(1000);
    connect(m_hopAdvance, &QTimer::timeout, this, [this]() {
        if (m_planState != RoutePlanState::AtStop) {
            stopHopAdvanceTimer();
            return;
        }
        if (m_vehicle && (m_vehicle->isShuttingDown() || m_vehicle->isStandBy()
                          || m_vehicle->hopOnActive())) {
            pausePlan();
            return;
        }
        if (m_hopSecondsLeft > 0) {
            --m_hopSecondsLeft;
            emit hopPromptChanged();
            if (m_hopSecondsLeft > 0)
                return;
        }
        advanceToNextHop();
    });

    restorePlan();

    // The plan lives in the settings hash, which settings-service may populate
    // a moment after this service is built. Retry once on the first settings
    // snapshot so a reboot resume does not depend on constructor ordering.
    connect(m_repo, &MdbRepository::fieldsUpdated, this,
            [this](const QString &channel, const FieldMap &) {
        if (channel == QLatin1String("settings") && !m_restoreChecked && !m_plan.isValid())
            restorePlan();
    });
}

void NavigationService::setMapService(MapService *map)
{
    if (m_map == map) return;
    m_map = map;
    if (m_map) {
        connect(m_map, &MapService::vehiclePositionChanged,
                this, &NavigationService::onVehiclePositionChanged);
    }
}

void NavigationService::onVehiclePositionChanged()
{
    if (m_status != NavigationStatus::Navigating &&
        m_status != NavigationStatus::Rerouting &&
        m_status != NavigationStatus::Arrived)
        return;

    if (!m_navigationCadence.advance())
        return;
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

QString NavigationService::currentSegmentStreetName() const
{
    if (!m_route.isValid()) return {};
    QString result;
    int bestShape = -1;
    for (const auto &instr : m_route.instructions) {
        if (instr.originalShapeIndex <= m_currentSegmentIndex &&
            instr.originalShapeIndex > bestShape) {
            bestShape = instr.originalShapeIndex;
            result = instr.streetName;
        }
    }
    return result;
}

bool NavigationService::hasCurrentEdgeAttrs() const
{
    return m_route.hasShapeAttrs() &&
           m_currentSegmentIndex >= 0 &&
           m_currentSegmentIndex < m_route.shapeAttrs.size();
}

QString NavigationService::currentEdgeName() const
{
    if (!hasCurrentEdgeAttrs()) return {};
    const auto &names = m_route.shapeAttrs[m_currentSegmentIndex].names;
    return names.isEmpty() ? QString() : names.first();
}

QStringList NavigationService::currentEdgeRefs() const
{
    if (!hasCurrentEdgeAttrs()) return {};
    const auto &names = m_route.shapeAttrs[m_currentSegmentIndex].names;
    if (names.size() <= 1) return {};
    return names.mid(1);
}

QString NavigationService::currentEdgeRoadClass() const
{
    if (!hasCurrentEdgeAttrs()) return {};
    return m_route.shapeAttrs[m_currentSegmentIndex].roadClass;
}

int NavigationService::currentEdgeSpeedLimitKph() const
{
    if (!hasCurrentEdgeAttrs()) return 0;
    return m_route.shapeAttrs[m_currentSegmentIndex].speedLimitKph;
}

bool NavigationService::currentEdgeIsTunnel() const
{
    if (!hasCurrentEdgeAttrs()) return false;
    return m_route.shapeAttrs[m_currentSegmentIndex].tunnel;
}

bool NavigationService::currentEdgeIsBridge() const
{
    if (!hasCurrentEdgeAttrs()) return false;
    return m_route.shapeAttrs[m_currentSegmentIndex].bridge;
}

QString NavigationService::currentVerbalInstruction() const
{
    if (m_upcomingInstructions.isEmpty()) return {};
    const auto &instr = m_upcomingInstructions.first();

    auto pick = [&](std::initializer_list<const QString *> candidates) -> QString {
        for (const auto *s : candidates)
            if (!s->isEmpty()) return *s;
        return {};
    };

    // Post-transition confirmation override. For a few seconds after the
    // rider crosses a maneuver, Valhalla's verbal_post ("Continue for 300 m
    // on Oak") is the most useful thing to show — better than the next
    // maneuver's far-off alert. Only surface it when the upcoming maneuver
    // is still comfortably far away; otherwise we'd be covering its alert.
    if (m_hasLastPassedManeuver
        && !m_lastPassedManeuver.verbalPostTransitionInstruction.isEmpty()
        && m_lastPassedAt.isValid()
        && m_lastPassedAt.elapsed() < PostWindowMs
        && instr.distance > PostMinUpcomingGap) {
        return m_lastPassedManeuver.verbalPostTransitionInstruction;
    }

    // Keep destination-side approach guidance until the arrival transition.
    if (currentIsArrive()) {
        return pick({&instr.verbalAlertInstruction,
                     &instr.verbalPreTransitionInstruction,
                     &instr.instructionText,
                     &instr.verbalSuccinctInstruction});
    }

    // Start family: rider is at t=0 on segment 0. Valhalla's succ/pre tend
    // to stuff a "Then turn…" tail in, which duplicates the separate next-
    // preview row. instruction ("Drive north on X.") is the cleanest.
    if (instr.isStart) {
        return pick({&instr.instructionText,
                     &instr.verbalPreTransitionInstruction,
                     &instr.verbalSuccinctInstruction,
                     &instr.verbalAlertInstruction});
    }

    // Regular maneuvers: distance-based stage with hysteresis set in
    // updateVerbalStage(). Only two bands of actual text: alert at far
    // range, pre everywhere else. verbal_succinct ("Turn left.") is
    // designed for voice prompts at the last moment; as persistent banner
    // text it's strictly less informative than verbal_pre ("Turn left onto
    // Zur Marktflagge."), and flipping between the two at the 50 m
    // boundary makes the banner feel churny. Keep the stage machine intact
    // for any future voice layer; the banner just ignores stage 2's
    // succinct preference.
    if (m_currentVerbalStage == 0) {
        return pick({&instr.verbalAlertInstruction,
                     &instr.verbalPreTransitionInstruction,
                     &instr.instructionText,
                     &instr.verbalSuccinctInstruction});
    }
    return pick({&instr.verbalPreTransitionInstruction,
                 &instr.verbalAlertInstruction,
                 &instr.instructionText,
                 &instr.verbalSuccinctInstruction});
}

QString NavigationService::currentCompactInstruction() const
{
    if (m_upcomingInstructions.isEmpty()) return {};
    const auto &instruction = m_upcomingInstructions.first();
    // Keep start instructions and the approach/arrival tense selection intact.
    if (instruction.isStart || currentIsArrive() || instruction.verbalSuccinctInstruction.isEmpty())
        return currentVerbalInstruction();
    return instruction.verbalSuccinctInstruction;
}

QString NavigationService::currentInstructionText() const
{
    if (m_upcomingInstructions.isEmpty()) return {};
    return m_upcomingInstructions.first().instructionText;
}

bool NavigationService::currentIsStart() const
{
    if (m_upcomingInstructions.isEmpty()) return false;
    return m_upcomingInstructions.first().isStart;
}

bool NavigationService::currentIsArrive() const
{
    if (m_upcomingInstructions.isEmpty()) return false;
    auto t = m_upcomingInstructions.first().type;
    return t == ManeuverType::Arrive ||
           t == ManeuverType::ArriveRight ||
           t == ManeuverType::ArriveLeft;
}

int NavigationService::roundaboutExitCount() const
{
    if (m_upcomingInstructions.isEmpty()) return 0;
    return m_upcomingInstructions.first().roundaboutExitCount;
}

namespace {

// Least-squares circle through a set of points, in a local east/north metric
// frame anchored on the first one. Kasa fit: linear in (cx, cy, c), so it is a
// 3x3 solve rather than an iteration.
struct RingFit {
    double lat = 0.0;
    double lon = 0.0;
    double radius = 0.0;
    double maxResidual = 0.0;
    double arcSpanDeg = 0.0;
    bool ok = false;
};

RingFit fitRing(const QList<LatLng> &pts)
{
    RingFit fit;
    if (pts.size() < 3)
        return fit;

    const double lat0 = pts.first().latitude;
    const double lon0 = pts.first().longitude;
    const double cosLat0 = std::cos(lat0 * M_PI / 180.0);
    const auto toE = [&](const LatLng &p) { return (p.longitude - lon0) * 111320.0 * cosLat0; };
    const auto toN = [&](const LatLng &p) { return (p.latitude - lat0) * 111320.0; };

    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0, sxz = 0, syz = 0, sz = 0;
    for (const auto &p : pts) {
        const double x = toE(p), y = toN(p), z = x * x + y * y;
        sx += x; sy += y;
        sxx += x * x; syy += y * y; sxy += x * y;
        sxz += x * z; syz += y * z; sz += z;
    }
    const double n = static_cast<double>(pts.size());
    const double m[3][3] = {{sxx, sxy, sx}, {sxy, syy, sy}, {sx, sy, n}};
    const double v[3] = {sxz * 0.5, syz * 0.5, sz * 0.5};

    const auto det3 = [](const double a[3][3]) {
        return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
             - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
             + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    };
    const double det = det3(m);
    if (std::abs(det) < 1e-9)
        return fit;

    double sol[3];
    for (int col = 0; col < 3; ++col) {
        double t[3][3];
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                t[r][c] = (c == col) ? v[r] : m[r][c];
        sol[col] = det3(t) / det;
    }
    const double cx = sol[0], cy = sol[1];
    const double rSq = sol[2] + cx * cx + cy * cy;
    if (!std::isfinite(rSq) || rSq <= 0.0)
        return fit;
    fit.radius = std::sqrt(rSq);
    fit.lat = lat0 + cy / 111320.0;
    fit.lon = lon0 + cx / (111320.0 * cosLat0);

    // How well the points sit on that circle, and how much of it they cover.
    // A short arc can be fitted tightly by a circle of almost any size, so the
    // residual alone does not say the fit is trustworthy — the span does.
    double prevAng = 0.0;
    bool havePrev = false;
    for (const auto &p : pts) {
        const double dx = toE(p) - cx, dy = toN(p) - cy;
        fit.maxResidual = std::max(fit.maxResidual, std::abs(std::hypot(dx, dy) - fit.radius));
        const double ang = std::atan2(dy, dx) * 180.0 / M_PI;
        if (havePrev) {
            double step = std::fmod(ang - prevAng + 540.0, 360.0) - 180.0;
            fit.arcSpanDeg += std::abs(step);
        }
        prevAng = ang;
        havePrev = true;
    }

    fit.ok = fit.radius >= 4.0 && fit.radius <= 90.0
             && fit.maxResidual <= std::max(2.5, 0.18 * fit.radius)
             && fit.arcSpanDeg >= 40.0;
    return fit;
}

double metersBetween(const LatLng &a, const LatLng &b)
{
    const double cosLat = std::cos(a.latitude * M_PI / 180.0);
    return std::hypot((b.longitude - a.longitude) * 111320.0 * cosLat,
                      (b.latitude - a.latitude) * 111320.0);
}

} // namespace

QVariantMap NavigationService::currentRoundaboutRender() const
{
    return m_roundaboutRender;
}

void NavigationService::updateRoundaboutRender()
{
    int enterInstrIdx = -1;
    int exitInstrIdx = -1;

    if (!m_upcomingInstructions.isEmpty()) {
        const RouteInstruction &current = m_upcomingInstructions.first();
        if (current.type == ManeuverType::RoundaboutEnter ||
            current.type == ManeuverType::RoundaboutExit) {
            // Find the Enter/Exit pair in m_route.instructions so the same icon
            // works while approaching (current=Enter) and while inside the ring
            // (current=Exit).
            for (int i = 0; i < m_route.instructions.size(); ++i) {
                if (m_route.instructions[i].originalShapeIndex != current.originalShapeIndex ||
                    m_route.instructions[i].type != current.type)
                    continue;
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
    }

    const int enterShape = (enterInstrIdx >= 0)
        ? m_route.instructions[enterInstrIdx].originalShapeIndex : -1;
    const int exitShape = (exitInstrIdx >= 0)
        ? m_route.instructions[exitInstrIdx].originalShapeIndex : -1;

    if (enterShape == m_roundaboutEnterShape && exitShape == m_roundaboutExitShape)
        return;

    m_roundaboutEnterShape = enterShape;
    m_roundaboutExitShape = exitShape;
    m_roundaboutRender = (enterInstrIdx >= 0 && exitInstrIdx >= 0)
        ? buildRoundaboutRender(enterInstrIdx, exitInstrIdx)
        : QVariantMap();
    emit roundaboutRenderChanged();
}

QVariantMap NavigationService::buildRoundaboutRender(int enterInstrIdx, int exitInstrIdx) const
{
    QVariantMap result;

    const int enterIdx = m_route.instructions[enterInstrIdx].originalShapeIndex;
    const int exitIdx = m_route.instructions[exitInstrIdx].originalShapeIndex;

    // Clamp and sanity.
    if (enterIdx < 0 || enterIdx >= m_route.waypoints.size())
        return result;
    if (exitIdx <= enterIdx || exitIdx >= m_route.waypoints.size())
        return result;

    // Arc points on the ring itself.
    QList<LatLng> arcPoints;
    arcPoints.reserve(exitIdx - enterIdx + 1);
    for (int i = enterIdx; i <= exitIdx; ++i)
        arcPoints.append(m_route.waypoints[i]);
    if (arcPoints.size() < 2)
        return result;

    const RingFit fit = fitRing(arcPoints);

    double centerLat, centerLon, ringRadius;
    if (fit.ok) {
        centerLat = fit.lat;
        centerLon = fit.lon;
        ringRadius = fit.radius;
    } else {
        // The arc does not pin the circle down (a first exit gives barely any
        // of it). The mean of on-ring points is still within a radius of the
        // real centre, which is good enough to anchor a tile query; QML refits
        // from the ring geometry it finds there.
        double latSum = 0, lonSum = 0;
        for (const auto &p : arcPoints) { latSum += p.latitude; lonSum += p.longitude; }
        centerLat = latSum / arcPoints.size();
        centerLon = lonSum / arcPoints.size();
        ringRadius = 0.0;
        for (const auto &p : arcPoints)
            ringRadius = std::max(ringRadius, metersBetween({centerLat, centerLon}, p));
        ringRadius = std::max(ringRadius, 12.0);
    }

    // Approach and exit stubs measured in metres rather than in waypoints:
    // Valhalla's shape density runs from a couple of metres in a tight curve to
    // hundreds on a straight, so a fixed waypoint count is a random distance.
    // Long enough that the exit stub still shows some road after the arrow head
    // has taken its bite out of it.
    // The far end is interpolated rather than rounded up to the next waypoint.
    // On a straight approach the next one can be 100 m further out, and that
    // overshoot eats the icon's whole framing budget.
    const double stubMeters = std::clamp(0.70 * ringRadius, 15.0, 60.0);
    // The approach is carried much further back than it is drawn, because the
    // icon aligns itself to the road the rider is on and the last stretch of
    // that road is already flaring into the junction. Measured on the bundled
    // routes, a bearing taken over the final 0.45R sits 23 degrees off the
    // road's true heading on average and 35 at worst; taken between 1.2R and
    // 2.6R back it is within 1.7 degrees.
    const double alignFarMeters = std::clamp(2.6 * ringRadius, 45.0, 200.0);
    const double alignNearMeters = std::clamp(1.2 * ringRadius, 20.0, 90.0);

    const auto stubFrom = [&](int anchor, int step, double metres) {
        QList<LatLng> out;
        double acc = 0.0;
        int i = anchor;
        while (i + step >= 0 && i + step < m_route.waypoints.size()) {
            const LatLng &a = m_route.waypoints[i];
            const LatLng &b = m_route.waypoints[i + step];
            const double seg = metersBetween(a, b);
            if (acc + seg >= metres) {
                const double t = (seg > 1e-6) ? (metres - acc) / seg : 0.0;
                out.append({a.latitude + (b.latitude - a.latitude) * t,
                            a.longitude + (b.longitude - a.longitude) * t});
                break;
            }
            acc += seg;
            i += step;
            out.append(b);
        }
        return out;
    };

    const QList<LatLng> approach = stubFrom(enterIdx, -1, alignFarMeters);
    const QList<LatLng> exitRun = stubFrom(exitIdx, +1, stubMeters);

    QList<LatLng> pathPoints;
    pathPoints.reserve(approach.size() + (exitIdx - enterIdx + 1) + exitRun.size());
    for (int i = approach.size() - 1; i >= 0; --i)
        pathPoints.append(approach[i]);
    for (int i = enterIdx; i <= exitIdx; ++i)
        pathPoints.append(m_route.waypoints[i]);
    pathPoints.append(exitRun);

    QVariantList path;
    path.reserve(pathPoints.size());
    for (const auto &pt : pathPoints) {
        QVariantList p;
        p << pt.latitude << pt.longitude;
        path.append(QVariant(p));
    }

    QVariantList arcPath;
    arcPath.reserve(arcPoints.size());
    for (const auto &pt : arcPoints) {
        QVariantList p;
        p << pt.latitude << pt.longitude;
        arcPath.append(QVariant(p));
    }

    result[QStringLiteral("centerLat")] = centerLat;
    result[QStringLiteral("centerLon")] = centerLon;
    result[QStringLiteral("ringRadius")] = ringRadius;
    // False means "anchor only, refit before you draw anything".
    result[QStringLiteral("ringValid")] = fit.ok;
    result[QStringLiteral("path")] = path;
    result[QStringLiteral("arcPath")] = arcPath;
    // Indices into path, so QML knows which stretch is on the ring and can take
    // the arrow direction off the exit road rather than off a canvas clip.
    result[QStringLiteral("entryIndex")] = approach.size();
    result[QStringLiteral("exitIndex")] = approach.size() + (exitIdx - enterIdx);
    result[QStringLiteral("stubMeters")] = stubMeters;
    // path[0] sits exactly alignFarMeters back; QML pairs it with the point
    // alignNearMeters back to get the road's heading clear of the flare.
    result[QStringLiteral("alignNearMeters")] = alignNearMeters;
    result[QStringLiteral("alignFarMeters")] = alignFarMeters;
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

    // A single destination is a one-stop plan. Everything else about starting
    // a trip now lives in the plan path.
    RouteStop stop;
    stop.position = {lat, lng};
    stop.label = address;
    setRoutePlan(QList<RouteStop>{stop}, 0);
}

void NavigationService::setRoutePlan(const QVariantList &stops, int startStep)
{
    QList<RouteStop> parsed;
    parsed.reserve(stops.size());
    for (const QVariant &entry : stops) {
        const QVariantMap map = entry.toMap();
        RouteStop stop;
        const QVariant lat = map.contains(QStringLiteral("latitude"))
            ? map.value(QStringLiteral("latitude"))
            : map.value(QStringLiteral("lat"));
        const QVariant lon = map.contains(QStringLiteral("longitude"))
            ? map.value(QStringLiteral("longitude"))
            : map.value(QStringLiteral("lon"));
        stop.position = {lat.toDouble(), lon.toDouble()};
        stop.label = map.value(QStringLiteral("label")).toString();
        if (stop.position.isValid())
            parsed.append(stop);
    }
    setRoutePlan(parsed, startStep);
}

void NavigationService::setRoutePlan(const QList<RouteStop> &stops, int startStep)
{
    QList<RouteStop> cleaned;
    cleaned.reserve(stops.size());
    for (const RouteStop &stop : stops) {
        if (stop.position.isValid())
            cleaned.append(stop);
    }
    if (cleaned.isEmpty()) {
        clearNavigation();
        return;
    }

    m_valhalla->cancelPending();
    stopHopAdvanceTimer();
    m_plan = RoutePlan();
    m_plan.stops = cleaned;
    RoutePlanService::assignIds(m_plan.stops);
    m_plan.currentStep = startStep;
    m_plan.clampStep();
    for (int i = 0; i < m_plan.currentStep; ++i)
        m_plan.stops[i].reached = true;

    persistPlan();

    // Recents track the trip's final stop, not each hop, so a multi-hop plan
    // adds one entry rather than one per intermediate stop.
    const RouteStop finalStop = m_plan.stops.last();
    emit destinationRequested(finalStop.position.latitude, finalStop.position.longitude,
                              finalStop.label);
    emit planChanged();
    beginCurrentHop(ValhallaClient::Reason::Destination);
}

void NavigationService::appendStop(double lat, double lng, const QString &label)
{
    RouteStop stop;
    stop.position = {lat, lng};
    stop.label = label;
    if (!stop.position.isValid())
        return;

    if (!m_plan.isValid()) {
        setRoutePlan(QList<RouteStop>{stop}, 0);
        return;
    }

    int maxId = 0;
    for (const RouteStop &existing : m_plan.stops)
        maxId = std::max(maxId, existing.id);
    stop.id = maxId + 1;
    m_plan.stops.append(stop);
    emit destinationRequested(stop.position.latitude, stop.position.longitude, stop.label);

    // A finished trip is re-opened by the new stop. When the rider had already
    // arrived at what was the last stop, that stop becomes a hop and the
    // continue prompt reappears instead of the addition sitting inert behind an
    // arrival.
    if (m_wasArrived && !m_plan.atLastStop()) {
        onHopReached();
        return;
    }
    if (m_planState == RoutePlanState::Complete) {
        beginCurrentHop(ValhallaClient::Reason::Destination);
        return;
    }

    persistPlan();
    emit planChanged();
}

void NavigationService::removeStop(int index)
{
    if (index < 0 || index >= m_plan.stopCount())
        return;

    m_plan.stops.removeAt(index);

    if (m_plan.stops.isEmpty()) {
        clearNavigation();
        return;
    }

    if (index < m_plan.currentStep) {
        --m_plan.currentStep;
        persistPlan();
        emit planChanged();
        return;
    }

    if (index == m_plan.currentStep) {
        if (m_plan.currentStep >= m_plan.stopCount()) {
            // Removed the last stop while guiding to it: the trip ends here.
            m_plan.currentStep = m_plan.stopCount() - 1;
            completePlan();
            return;
        }
        persistPlan();
        emit planChanged();
        beginCurrentHop(ValhallaClient::Reason::Destination);
        return;
    }

    persistPlan();
    emit planChanged();
}

void NavigationService::moveStop(int from, int to)
{
    if (from < 0 || from >= m_plan.stopCount())
        return;
    if (to < 0 || to >= m_plan.stopCount())
        return;
    if (from == to)
        return;

    // Follow the current target by id so a reorder never silently retargets.
    const int currentId = m_plan.currentStop().id;
    m_plan.stops.move(from, to);
    const int newIndex = m_plan.indexOfStopId(currentId);
    const bool targetMoved = newIndex >= 0 && newIndex != m_plan.currentStep;
    if (newIndex >= 0)
        m_plan.currentStep = newIndex;

    persistPlan();
    emit planChanged();
    if (targetMoved)
        beginCurrentHop(ValhallaClient::Reason::Destination);
}

void NavigationService::skipCurrentStop()
{
    if (!m_plan.isValid())
        return;
    advanceToNextHop();
}

void NavigationService::jumpToStop(int index)
{
    if (index < 0 || index >= m_plan.stopCount())
        return;
    stopHopAdvanceTimer();
    m_plan.currentStep = index;
    for (int i = 0; i < m_plan.stopCount(); ++i)
        m_plan.stops[i].reached = (i < index);
    persistPlan();
    emit planChanged();
    beginCurrentHop(ValhallaClient::Reason::Destination);
}

void NavigationService::confirmContinue()
{
    if (m_planState != RoutePlanState::AtStop)
        return;
    advanceToNextHop();
}

void NavigationService::declineContinue()
{
    if (m_planState != RoutePlanState::AtStop)
        return;
    stopHopAdvanceTimer();
    setStatus(NavigationStatus::Idle);
    setPlanState(RoutePlanState::Held);
}

void NavigationService::pausePlan()
{
    if (m_planState != RoutePlanState::Navigating && m_planState != RoutePlanState::AtStop)
        return;

    m_pausedAfterReach = (m_planState == RoutePlanState::AtStop);
    if (m_pausedAfterReach && m_plan.currentStep < m_plan.stopCount())
        m_plan.stops[m_plan.currentStep].reached = true;

    stopHopAdvanceTimer();
    m_valhalla->cancelPending();
    persistPlan();
    setStatus(NavigationStatus::Idle);
    setPlanState(RoutePlanState::Paused);
}

void NavigationService::resumePlan()
{
    if (!m_plan.isValid())
        return;

    if (m_planState == RoutePlanState::Held) {
        // The rider declined at this stop. Resuming re-asks rather than
        // skipping; skipCurrentStop() is the explicit way to move on.
        m_wasArrived = true;
        setStatus(NavigationStatus::Arrived);
        setPlanState(RoutePlanState::AtStop);
        startHopAdvanceTimer();
        return;
    }

    if (m_planState == RoutePlanState::Paused) {
        if (m_pausedAfterReach) {
            if (m_plan.atLastStop()) {
                completePlan();
                return;
            }
            advanceToNextHop();
            return;
        }
        beginCurrentHop(ValhallaClient::Reason::Recovery);
        return;
    }

    if (m_planState == RoutePlanState::None || m_planState == RoutePlanState::Complete)
        beginCurrentHop(ValhallaClient::Reason::Recovery);
}

void NavigationService::persistPlan()
{
    if (m_plan.isValid())
        m_planService.save(m_plan, true);
    else
        m_planService.clear();
}

void NavigationService::beginCurrentHop(ValhallaClient::Reason reason)
{
    if (!m_plan.isValid())
        return;

    // Tear down any prior hop/route state before starting the next one. Without
    // this the old polyline, arrival pill, and stale distances linger on the
    // map until the new route comes back from the router.
    m_valhalla->cancelPending();
    stopHopAdvanceTimer();
    m_route = Route();
    m_upcomingInstructions.clear();
    m_distanceToDestination = 0;
    m_remainingDuration = 0;
    m_distanceFromRoute = 0;
    m_isOffRoute = false;
    m_rerouteGate.reset();
    m_rerouteRetry->stop();
    m_navigationCadence.reset();
    m_wasArrived = false;
    m_pausedAfterReach = false;
    emit arrivalReset();
    m_currentSegmentIndex = 0;
    m_hasLastPassedManeuver = false;
    m_prevLeadingShapeIdx = -1;
    if (m_status == NavigationStatus::Arrived || m_status == NavigationStatus::Navigating
        || m_status == NavigationStatus::Rerouting) {
        setStatus(NavigationStatus::Idle);
    }
    clearError();
    emit routeChanged();
    emit instructionChanged();
    emit positionChanged();
    updateRoundaboutRender();

    const RouteStop target = m_plan.currentStop();
    m_destination = target.position;
    m_destAddress = target.label;
    setPlanState(RoutePlanState::Navigating);
    writePlanToNavigationHash();
    emit destinationChanged();
    refreshPlanOverview();

    if (!selectRouteOrigin().isValid()) {
        qDebug() << "NavigationService: waiting for a trustworthy position before calculating route";
        return;
    }
    setStatus(NavigationStatus::Calculating);
    requestRoute(reason);
}

void NavigationService::onHopReached()
{
    RouteInstruction arrival;
    arrival.type = ManeuverType::Arrive;
    for (auto it = m_route.instructions.crbegin(); it != m_route.instructions.crend(); ++it) {
        if (it->type == ManeuverType::Arrive || it->type == ManeuverType::ArriveLeft
            || it->type == ManeuverType::ArriveRight) {
            arrival = *it;
            break;
        }
    }
    arrival.distance = 0;
    m_upcomingInstructions = {arrival};
    m_hasLastPassedManeuver = false;
    m_wasArrived = true;
    m_remainingDuration = 0;
    m_isOffRoute = false;
    m_rerouteRetry->stop();
    m_valhalla->cancelPending();

    if (m_plan.currentStep < m_plan.stopCount())
        m_plan.stops[m_plan.currentStep].reached = true;
    persistPlan();

    setStatus(NavigationStatus::Arrived);
    emit instructionChanged();
    emit positionChanged();
    updateRoundaboutRender();
    emit planChanged();
    setPlanState(RoutePlanState::AtStop);
    emit hopReached(m_plan.currentStep, m_plan.currentStop().label);
    startHopAdvanceTimer();
}

void NavigationService::advanceToNextHop()
{
    stopHopAdvanceTimer();
    if (!m_plan.isValid())
        return;
    if (m_plan.atLastStop()) {
        completePlan();
        return;
    }
    ++m_plan.currentStep;
    beginCurrentHop(ValhallaClient::Reason::Destination);
    emit planChanged();
}

void NavigationService::completePlan()
{
    stopHopAdvanceTimer();
    m_valhalla->cancelPending();
    m_valhalla->cancelPreview();
    clearPlanOverview();
    m_planService.clear();
    setStatus(NavigationStatus::Idle);
    setPlanState(RoutePlanState::Complete);
    emit planChanged();
}

void NavigationService::startHopAdvanceTimer()
{
    m_hopSecondsLeft = HopAdvanceTimeoutSeconds;
    emit hopPromptChanged();
    m_hopAdvance->start();
}

void NavigationService::stopHopAdvanceTimer()
{
    if (m_hopAdvance)
        m_hopAdvance->stop();
    if (m_hopSecondsLeft != 0) {
        m_hopSecondsLeft = 0;
        emit hopPromptChanged();
    }
}

void NavigationService::setPlanState(RoutePlanState state)
{
    if (m_planState == state)
        return;
    m_planState = state;
    emit planStateChanged();
}

void NavigationService::restorePlan()
{
    if (m_plan.isValid()) {
        m_restoreChecked = true;
        return;
    }

    RoutePlan stored = m_planService.load();
    m_restoreChecked = true;
    if (!stored.isValid() || !m_planService.loadedActive())
        return;

    m_plan = stored;
    // A stop already marked reached means the trip was paused at a hop, so
    // resuming should advance rather than guide back to it.
    m_pausedAfterReach = m_plan.currentStop().reached;
    m_destination = m_plan.currentStop().position;
    m_destAddress = m_plan.currentStop().label;
    emit planChanged();
    emit destinationChanged();
    setPlanState(RoutePlanState::Paused);
}

void NavigationService::writePlanToNavigationHash()
{
    if (!m_plan.isValid())
        return;

    const RouteStop target = m_plan.currentStop();
    const QString lat = QString::number(target.position.latitude, 'f', 6);
    const QString lon = QString::number(target.position.longitude, 'f', 6);
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("latitude"), lat);
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("longitude"), lon);
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("address"), target.label);
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("timestamp"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("destination"),
                lat + QLatin1Char(',') + lon);
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("waypoints"),
                RoutePlanService::serializeWaypoints(m_plan.stops));
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("current-step"),
                QString::number(m_plan.currentStep));
    m_repo->publish(QStringLiteral("navigation"), QStringLiteral("updated"));
}

void NavigationService::clearPlanOverview()
{
    if (m_planOverview.isEmpty() && m_planGeometry.isEmpty()
        && m_planTotalDistance == 0 && m_planTotalDuration == 0)
        return;
    m_planOverview.clear();
    m_planGeometry.clear();
    m_planTotalDistance = 0;
    m_planTotalDuration = 0;
    emit planOverviewChanged();
}

void NavigationService::refreshPlanOverview()
{
    clearPlanOverview();
    if (!m_plan.isValid())
        return;

    // The preview covers what remains: the current position to the stop being
    // guided to, then each later stop. One multi-stop request, parsed per leg.
    QList<LatLng> remaining;
    for (int i = m_plan.currentStep; i < m_plan.stopCount(); ++i)
        remaining.append(m_plan.stops.at(i).position);
    if (remaining.isEmpty())
        return;

    RouteOrigin origin = selectRouteOrigin();
    if (!origin.isValid()) {
        // Without a trustworthy position, preview the legs between stops. The
        // degenerate first leg maps to a zero-length row and is harmless.
        origin.position = remaining.first();
        origin.radiusMeters = 150;
    }
    m_valhalla->requestPreviewRoute(origin, remaining);
}

void NavigationService::onPlanPreviewReady(const QList<Route> &legs)
{
    if (!m_plan.isValid())
        return;

    m_plan.hops.clear();
    m_planOverview.clear();
    m_planGeometry.clear();
    m_planTotalDistance = 0;
    m_planTotalDuration = 0;

    for (int i = 0; i < legs.size(); ++i) {
        const int toIndex = m_plan.currentStep + i;
        if (toIndex < 0 || toIndex >= m_plan.stopCount())
            break;
        // Leg 0 starts at the rider's position, later legs at the stop before.
        const int fromIndex = (i == 0) ? -1 : toIndex - 1;

        HopPreview hop;
        hop.fromStopId = fromIndex >= 0 ? m_plan.stops.at(fromIndex).id : -1;
        hop.toStopId = m_plan.stops.at(toIndex).id;
        hop.distance = legs.at(i).distance;
        hop.duration = legs.at(i).duration;
        hop.route = legs.at(i);
        hop.ready = true;
        m_plan.hops.append(hop);

        m_planTotalDistance += hop.distance;
        m_planTotalDuration += hop.duration;

        QVariantMap entry;
        entry[QStringLiteral("fromIndex")] = fromIndex;
        entry[QStringLiteral("toIndex")] = toIndex;
        entry[QStringLiteral("fromLabel")] =
            fromIndex >= 0 ? m_plan.stops.at(fromIndex).label : QString();
        entry[QStringLiteral("toLabel")] = m_plan.stops.at(toIndex).label;
        entry[QStringLiteral("distance")] = hop.distance;
        entry[QStringLiteral("duration")] = hop.duration;
        entry[QStringLiteral("ready")] = true;
        m_planOverview.append(entry);

        // Merge the leg geometry, dropping the vertex shared with the previous
        // leg, so the overview can frame the whole remaining plan.
        const QList<LatLng> &points = legs.at(i).waypoints;
        if (!points.isEmpty()) {
            if (!m_planGeometry.isEmpty() && m_planGeometry.last() == points.first())
                m_planGeometry.append(points.mid(1));
            else
                m_planGeometry.append(points);
        }
    }

    emit planOverviewChanged();
}

void NavigationService::onPlanPreviewFailed(const QString &error)
{
    qDebug() << "NavigationService: plan preview failed -" << error;
    if (!m_plan.isValid())
        return;

    // Keep one not-ready row per remaining hop so the overview still lists the
    // stops, just without times, instead of vanishing.
    m_plan.hops.clear();
    m_planOverview.clear();
    m_planGeometry.clear();
    m_planTotalDistance = 0;
    m_planTotalDuration = 0;
    for (int i = m_plan.currentStep; i < m_plan.stopCount(); ++i) {
        const int fromIndex = (i == m_plan.currentStep) ? -1 : i - 1;
        QVariantMap entry;
        entry[QStringLiteral("fromIndex")] = fromIndex;
        entry[QStringLiteral("toIndex")] = i;
        entry[QStringLiteral("fromLabel")] =
            fromIndex >= 0 ? m_plan.stops.at(fromIndex).label : QString();
        entry[QStringLiteral("toLabel")] = m_plan.stops.at(i).label;
        entry[QStringLiteral("distance")] = 0.0;
        entry[QStringLiteral("duration")] = 0.0;
        entry[QStringLiteral("ready")] = false;
        m_planOverview.append(entry);
    }
    emit planOverviewChanged();
}

QVariantList NavigationService::planStops() const
{
    QVariantList list;
    list.reserve(m_plan.stops.size());
    for (const RouteStop &stop : m_plan.stops) {
        QVariantMap entry;
        entry[QStringLiteral("id")] = stop.id;
        entry[QStringLiteral("latitude")] = stop.position.latitude;
        entry[QStringLiteral("longitude")] = stop.position.longitude;
        entry[QStringLiteral("label")] = stop.label;
        entry[QStringLiteral("reached")] = stop.reached;
        list.append(entry);
    }
    return list;
}

void NavigationService::clearNavigation()
{
    // Ingest calls this on an external clear. Nothing to do when already idle
    // with no plan, which also stops an echo from re-entering.
    if (!m_plan.isValid() && !m_destination.isValid() && m_status == NavigationStatus::Idle)
        return;

    m_valhalla->cancelPending();
    stopHopAdvanceTimer();
    m_route = Route();
    m_destination = {};
    m_destAddress.clear();
    m_upcomingInstructions.clear();
    m_distanceToDestination = 0;
    m_remainingDuration = 0;
    m_distanceFromRoute = 0;
    m_isOffRoute = false;
    m_rerouteGate.reset();
    m_rerouteRetry->stop();
    m_navigationCadence.reset();
    m_wasArrived = false;
    m_pausedAfterReach = false;
    emit arrivalReset();
    m_currentSegmentIndex = 0;
    m_hasLastPassedManeuver = false;
    m_prevLeadingShapeIdx = -1;
    m_plan = RoutePlan();

    m_valhalla->cancelPreview();
    clearPlanOverview();
    m_planService.clear();
    setStatus(NavigationStatus::Idle);
    clearError();
    emit routeChanged();
    emit destinationChanged();
    emit instructionChanged();
    emit positionChanged();
    updateRoundaboutRender();
    emit planChanged();
    setPlanState(RoutePlanState::None);

    // Clear Redis. Set fields to "" rather than HDEL: HiredisWorker::doHdel
    // does not publish a notification, so subscribers (bluetooth-service's
    // HashWatcher, our own SyncableStore pub/sub path) never wake up and
    // would only learn of the clear via the 5-second HGETALL poll.
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("latitude"), QString());
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("longitude"), QString());
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("address"), QString());
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("timestamp"), QString());
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("destination"), QString());
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("waypoints"), QString());
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("current-step"), QString());
    m_repo->publish(QStringLiteral("navigation"), QStringLiteral("cleared"));
}

void NavigationService::setRoute(const Route &route)
{
    // An injected route supersedes any real request still queued, so a late
    // dispatch cannot clobber it (simulator and tests inject routes).
    m_valhalla->cancelPending();
    m_wasArrived = false;
    emit arrivalReset();
    // A plan supplies the target; only an injected standalone route derives it
    // from the shape's last point.
    if (!m_plan.isValid())
        m_destination = {};
    m_activeRouteReason = ValhallaClient::Reason::Initial;
    onRouteCalculated(route);
}

// --- Slot handlers ---

void NavigationService::onGpsChanged()
{
    // Update position when GPS fix is recent; keep last known position when stale
    // so navigation and dead reckoning can continue through brief GPS gaps
    if (!m_gps) return;

    if (!m_gps->currentSample().hasValidCoordinate())
        return;

    // Nav state is normally driven by MapService's DR tick via
    // onVehiclePositionChanged. Only run here as a fallback when DR isn't
    // initialised or is paused (e.g. simulator freeze), so boot + frozen
    // states still react to GPS edges.
    bool drReady = m_map && m_map->hasVehiclePosition();
    if (!drReady &&
        (m_status == NavigationStatus::Navigating ||
         m_status == NavigationStatus::Rerouting ||
         m_status == NavigationStatus::Arrived)) {
        updateNavigationState();
    }

    // Recovery: destination loaded but route not yet calculated (GPS wasn't
    // ready). Never runs for a held, paused, or finished plan: those keep their
    // route or deliberately have none, and must not silently resume.
    const bool planGuiding = m_planState == RoutePlanState::None
                          || m_planState == RoutePlanState::Planning
                          || m_planState == RoutePlanState::Navigating;
    if (planGuiding && m_destination.isValid() && !m_route.isValid() &&
        (m_status == NavigationStatus::Idle || m_status == NavigationStatus::Error)) {
        if (selectRouteOrigin().isValid()) {
            LatLng pos = currentPosition();
            double dist = pos.distanceTo(m_destination);
            if (dist < ArrivalProximity) {
                if (m_plan.isValid() && !m_plan.atLastStop())
                    onHopReached();
                else
                    clearNavigation();
            } else {
                requestRoute(ValhallaClient::Reason::Recovery);
            }
        }
    }
}

void NavigationService::onNavigationDataChanged()
{
    if (!m_nav) return;

    // Legacy target fields. They are both the single-destination request path
    // and the pointer to the current hop of a plan.
    double lat = m_nav->latitude().toDouble();
    double lng = m_nav->longitude().toDouble();
    if (lat == 0 && lng == 0 && !m_nav->destination().isEmpty()) {
        QStringList parts = m_nav->destination().split(QLatin1Char(','));
        if (parts.size() == 2) {
            lat = parts[0].toDouble();
            lng = parts[1].toDouble();
        }
    }
    const LatLng incomingTarget{lat, lng};
    const bool hasTarget = incomingTarget.isValid();

    bool waypointsOk = false;
    const QList<RouteStop> incomingStops =
        RoutePlanService::parseWaypoints(m_nav->waypoints(), &waypointsOk);

    // Echo guard. A write whose target already matches the active destination
    // changes nothing: it is either our own hash write-back or a route injected
    // with setRoute() (simulator, tests) that never had a plan. Without this a
    // store reconnect would re-arm arrival and fire arrived() twice.
    if (hasTarget && m_destination.isValid() && incomingTarget == m_destination) {
        const bool legacyEcho = !waypointsOk;
        const bool sameExistingPlan = m_plan.isValid()
            && planStopsEqual(incomingStops, m_plan.stops);
        const bool singleStopEcho = waypointsOk && incomingStops.size() == 1
            && incomingStops.first().position == m_destination;
        if (legacyEcho || sameExistingPlan || singleStopEcho)
            return;
    }

    if (waypointsOk) {
        if (!planStopsEqual(incomingStops, m_plan.stops)) {
            // A new plan was pushed. The step in the same write may point at a
            // specific hop; setRoutePlan() clamps it.
            setRoutePlan(incomingStops, m_nav->currentStep().toInt());
            return;
        }

        const bool targetMatches = hasTarget && incomingTarget == m_plan.currentStop().position;
        if (!targetMatches) {
            if (hasTarget) {
                // Same stops but the pointer moved elsewhere: an external
                // single destination replaced the plan.
                RouteStop stop;
                stop.position = incomingTarget;
                stop.label = m_nav->address();
                setRoutePlan(QList<RouteStop>{stop}, 0);
            } else if (m_plan.isValid()) {
                clearNavigation();
            }
            return;
        }

        // Same stops and the target is the current hop: either our own echo or
        // an external step jump. Honour a valid step change, ignore the echo.
        const int step = m_nav->currentStep().toInt();
        if (m_plan.isValid() && step != m_plan.currentStep
            && step >= 0 && step < m_plan.stopCount()) {
            m_plan.currentStep = step;
            persistPlan();
            emit planChanged();
            beginCurrentHop(ValhallaClient::Reason::Destination);
        }
        return;
    }

    // No plan payload: the legacy single-destination path.
    if (!hasTarget) {
        if (m_plan.isValid() || m_destination.isValid())
            clearNavigation();
        return;
    }

    // Echo of our own hop write, which also sets the legacy fields.
    if (m_plan.isValid() && incomingTarget == m_plan.currentStop().position)
        return;

    // Capture externally-pushed destinations (cloud / bluetooth / wwan, all of
    // which land on the navigation channel) as recents, same as locally-chosen
    // ones. setRoutePlan() emits destinationRequested for the plan's final stop.
    RouteStop stop;
    stop.position = incomingTarget;
    stop.label = m_nav->address();
    setRoutePlan(QList<RouteStop>{stop}, 0);
}

void NavigationService::onVehicleStateChanged()
{
    if (!m_vehicle) return;

    // Leaving the scooter (full lock, standby, or hop-on) ends a single trip.
    // HopOnLearning is excluded: the rider is still present, teaching the
    // scooter the combo.
    bool isLeaving = m_vehicle->isShuttingDown() || m_vehicle->isStandBy()
                  || m_vehicle->hopOnActive();
    if (!isLeaving)
        return;

    // An intermediate stop only pauses a multi-hop trip. The plan, its step,
    // and the prompt survive the stop so the rider can continue later.
    if (m_plan.isValid() && !m_plan.atLastStop()) {
        if (m_planState == RoutePlanState::Navigating || m_planState == RoutePlanState::AtStop) {
            qDebug() << "NavigationService: pausing plan at hop" << m_plan.currentStep;
            pausePlan();
        }
        return;
    }

    if (!m_destination.isValid())
        return;

    // Clear navigation when the rider is leaving the scooter near the
    // destination, or after the final arrival.
    LatLng pos = currentPosition();
    double dist = pos.distanceTo(m_destination);
    if (dist < ShutdownProximity || m_wasArrived) {
        qDebug() << "NavigationService: clearing navigation (leaving scooter near destination)";
        clearNavigation();
    }
}

void NavigationService::onRouteCalculated(const Route &route)
{
    if (m_wasArrived)
        return;
    m_route = route;
    // Shape indices identify a pair only within one route. A reroute can reuse
    // them for a different ring, and GPS may not yet permit rebuilding it.
    m_roundaboutEnterShape = m_roundaboutExitShape = -1;
    m_roundaboutRender.clear();
    emit roundaboutRenderChanged();
    m_remainingDuration = route.duration;
    m_currentSegmentIndex = 0;
    m_routeStartedAt.restart();
    m_hasLastPassedManeuver = false;
    m_prevLeadingShapeIdx = -1;
    m_isOffRoute = false;
    m_rerouteGate.reset();
    m_rerouteRetry->stop();
    m_navigationCadence.reset();

    if (!m_destination.isValid() && !route.waypoints.isEmpty()) {
        m_destination = route.waypoints.last();
        emit destinationChanged();
    }

    setStatus(NavigationStatus::Navigating);
    clearError();
    // MapService handles routeChanged synchronously and recomputes distance
    // from the independent physical pose. If this route was correlated onto
    // the wrong road, the update below immediately starts a new deviation
    // episode; the retry timer then asks again from a fresher origin after the
    // router cooldown instead of trying to judge the response in isolation.
    emit routeChanged();

    // Immediately update with whichever physical position supplied the route
    // origin (fresh GPS or a still-certain estimator fallback).
    if (currentPosition().isValid()) {
        updateNavigationState();
    }

    qDebug() << "NavigationService: route calculated -"
             << route.waypoints.size() << "waypoints,"
             << route.instructions.size() << "instructions,"
             << (route.distance / 1000.0) << "km";
}

void NavigationService::onRouteAttributesReady(const QList<EdgeAttrs> &attrs)
{
    if (!m_route.isValid())
        return;
    int expected = m_route.waypoints.size() - 1;
    if (attrs.size() != expected) {
        qDebug() << "NavigationService: trace_attributes size mismatch — got"
                 << attrs.size() << "want" << expected
                 << "(ignoring, falling back to tile path)";
        return;
    }
    m_route.shapeAttrs = attrs;
    emit routeAttributesChanged();
    qDebug() << "NavigationService: route enriched with" << attrs.size()
             << "edge attribute slots";
}

void NavigationService::onRouteError(const QString &error)
{
    if (m_activeRouteReason == ValhallaClient::Reason::Reroute
        && m_route.isValid()) {
        setStatus(NavigationStatus::Navigating);
        armRerouteRetry();
        qWarning() << "NavigationService: reroute failed; retaining existing route -"
                   << error;
        return;
    }
    raiseError(error);
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
            raiseError(QStringLiteral("Too many routing requests"));
            return;
        }
        if (cause == ValhallaClient::RejectionCause::Unhealthy) {
            raiseError(QStringLiteral("Cannot reach routing server"));
            return;
        }
    }

    // A rejected reroute was never sent, so keep the old route visible and
    // reopen the episode gate when the client's cooldown has elapsed.
    if (reason == ValhallaClient::Reason::Reroute &&
        m_status == NavigationStatus::Rerouting && m_route.isValid()) {
        setStatus(NavigationStatus::Navigating);
    }
    if (reason == ValhallaClient::Reason::Reroute)
        armRerouteRetry();

    qDebug() << "NavigationService: request rejected reason=" << static_cast<int>(reason)
             << "cause=" << static_cast<int>(cause);
}

void NavigationService::onRequestDispatched(ValhallaClient::Reason reason)
{
    m_activeRouteReason = reason;
    if (reason == ValhallaClient::Reason::Reroute && m_isOffRoute
        && m_route.isValid()) {
        setStatus(NavigationStatus::Rerouting);
    }
}

// --- Internal ---

void NavigationService::updateNavigationState()
{
    // Arrival guidance belongs to this route until clear/end or an explicit new route.
    if (m_wasArrived || !m_route.isValid()) return;

    LatLng pos = currentPosition();
    if (!pos.isValid()) return;

    // Straight-line distance to the destination point, used only for arrival
    // detection below. "Am I physically next to the destination" is the right
    // question there, independent of how the route gets there.
    double straightLineToDestination = pos.distanceTo(m_destination);

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

    // Default distance to destination: along-route geometry from the snapped
    // position to the final waypoint. The normal path below overrides this with
    // a maneuver-length sum (kept consistent with the remaining-duration
    // figure); this default covers the arrival-return path and the rare tick
    // with no upcoming maneuver. Either form is along-route, never great-circle,
    // so it never reads shorter than the next-maneuver distance.
    m_distanceToDestination = RouteHelpers::remainingDistanceAlongRoute(
        m_snappedPosition, m_route.waypoints, m_currentSegmentIndex);

    // Use the actual final maneuver, even if the proximity threshold was crossed
    // between GPS ticks before the upcoming-instruction walker reached it.
    if (straightLineToDestination < ArrivalProximity) {
        // An intermediate hop is not the end of the trip: it hands over to the
        // continue prompt and the plan keeps its step.
        if (m_plan.isValid() && !m_plan.atLastStop()) {
            onHopReached();
            return;
        }

        RouteInstruction arrival;
        arrival.type = ManeuverType::Arrive;
        for (auto it = m_route.instructions.crbegin(); it != m_route.instructions.crend(); ++it) {
            if (it->type == ManeuverType::Arrive || it->type == ManeuverType::ArriveLeft
                || it->type == ManeuverType::ArriveRight) {
                arrival = *it;
                break;
            }
        }
        arrival.distance = 0;
        m_upcomingInstructions = {arrival};
        m_hasLastPassedManeuver = false;
        m_wasArrived = true;
        m_remainingDuration = 0;
        m_isOffRoute = false;
        m_rerouteRetry->stop();
        m_valhalla->cancelPending();
        setStatus(NavigationStatus::Arrived);
        emit instructionChanged();
        emit positionChanged();
        updateRoundaboutRender();
        if (m_plan.isValid()) {
            // Final stop: the trip is done, so drop the persisted plan. The
            // in-memory plan stays until the rider leaves, keeping the arrival
            // card and the leave-near-destination check intact.
            m_planService.clear();
            setPlanState(RoutePlanState::Complete);
        }
        emit arrived();
        return;
    }

    // Off-route detection with hysteresis to prevent boundary oscillation.
    // Presentation deliberately stays route-snapped until this authoritative
    // physical-distance policy fires, so visual release never makes rerouting
    // start sooner or creates an unexplained off-route gap.
    m_isOffRoute = RouteDistancePolicy::update(
        m_isOffRoute, distFromRoute, OffRouteTolerance, OnRouteTolerance);

    // Queue one reroute per deviation episode. The status only changes after
    // ValhallaClient confirms dispatch, so a debounced/rejected request cannot
    // leave the UI stuck on "Recalculating".
    if (m_isOffRoute) {
        const RouteOrigin origin = selectRouteOrigin();
        if (m_rerouteGate.shouldRequest(true, origin.isValid())
            && m_destination.isValid()) {
            m_valhalla->requestRoute(origin, m_destination,
                                     ValhallaClient::Reason::Reroute);
        }
    } else if (!m_isOffRoute && m_status == NavigationStatus::Error) {
        m_rerouteGate.shouldRequest(false, false);
        m_rerouteRetry->stop();
        clearError();
    } else {
        m_rerouteGate.shouldRequest(false, false);
        m_rerouteRetry->stop();
    }

    // Find upcoming instructions. Pass the snapped position (not raw pos) so
    // along-route distance starts from the projection onto the current
    // segment. For the fallback path where MapService state isn't available,
    // m_snappedPosition was set to the local global-nearest projection above.
    //
    // hideStart: drop the kStart-family maneuver once the rider has either
    // advanced past segment 0 (they're genuinely moving along the route) or
    // been sitting in segment 0 for more than StartMaxLingerMs (the route
    // was calculated but they haven't started, or segment 0 is very long).
    bool hideStart = (m_currentSegmentIndex > 0) ||
                     (m_routeStartedAt.isValid() &&
                      m_routeStartedAt.elapsed() > StartMaxLingerMs);
    auto upcoming = RouteHelpers::findUpcomingInstructions(
        m_snappedPosition, m_route, m_currentSegmentIndex, 3, hideStart);

    // Post-transition tracking: if the leading-maneuver shape index has
    // advanced since last tick, the previous leading maneuver has just been
    // crossed. Stash it (with its verbal_post) so the verbal-text picker can
    // surface "Continue for X m on Oak" briefly before the next alert lands.
    int newLeadingShape = upcoming.isEmpty() ? -1 : upcoming.first().originalShapeIndex;
    if (m_prevLeadingShapeIdx >= 0
        && newLeadingShape != m_prevLeadingShapeIdx
        && !m_upcomingInstructions.isEmpty()) {
        // The prior leading instruction is what just got passed.
        const RouteInstruction &justPassed = m_upcomingInstructions.first();
        if (justPassed.originalShapeIndex == m_prevLeadingShapeIdx && !justPassed.isStart) {
            m_lastPassedManeuver = justPassed;
            m_hasLastPassedManeuver = true;
            m_lastPassedAt.start();
        }
    }
    m_prevLeadingShapeIdx = newLeadingShape;

    if (upcoming != m_upcomingInstructions) {
        m_upcomingInstructions = upcoming;
        emit instructionChanged();
    }
    updateRoundaboutRender();

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
            double remainingDist = 0;

            // Full duration and leg length of the upcoming maneuver + all after
            for (int i = firstIdx; i < m_route.instructions.size(); ++i) {
                remaining += m_route.instructions[i].duration;
                remainingDist += m_route.instructions[i].distance;
            }

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

            // Distance to destination, mirroring the remaining-duration figure:
            // the along-route distance to the upcoming maneuver (the same value
            // the TBT banner shows) plus the Valhalla leg length of that
            // maneuver and every one after it. Built from upcoming.first(), so
            // it is always >= the displayed next-maneuver distance.
            m_distanceToDestination = upcoming.first().distance + remainingDist;
        }
    }

    emit positionChanged();
}

void NavigationService::setStatus(NavigationStatus status)
{
    if (m_status != status) {
        if (m_status == NavigationStatus::Error)
            m_errorLinger->stop();
        m_status = status;
        emit statusChanged();
    }
}

void NavigationService::raiseError(const QString &message)
{
    m_errorMessage = message;
    setStatus(NavigationStatus::Error);
    m_errorLinger->start();
    emit errorChanged();
}

void NavigationService::clearError()
{
    m_errorLinger->stop();
    if (m_status == NavigationStatus::Error) {
        setStatus(m_route.isValid() ? NavigationStatus::Navigating
                                    : NavigationStatus::Idle);
    }
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorChanged();
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
    const GpsSample sample = m_gps->currentSample();
    return {sample.latitude, sample.longitude};
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

RouteOrigin NavigationService::selectRouteOrigin() const
{
    RerouteOriginSelector::Input input;
    if (m_gps) {
        input.gps = m_gps->currentSample();
        input.gpsAgeMs = m_gps->hasTimestamp() ? m_gps->timestampAgeMs() : -1;
    }
    if (m_map && m_map->hasVehiclePosition()) {
        input.physicalEstimate = {m_map->vehicleLatitude(),
                                  m_map->vehicleLongitude()};
        input.physicalUncertaintyMeters = m_map->positionUncertaintyMeters();
    }
    return RerouteOriginSelector::select(input);
}

bool NavigationService::requestRoute(ValhallaClient::Reason reason)
{
    if (!m_destination.isValid())
        return false;
    const RouteOrigin origin = selectRouteOrigin();
    if (!origin.isValid())
        return false;
    m_valhalla->requestRoute(origin, m_destination, reason);
    return true;
}

void NavigationService::armRerouteRetry()
{
    if (m_isOffRoute && m_route.isValid() && !m_rerouteRetry->isActive())
        m_rerouteRetry->start();
}

QString NavigationService::gpsDepartureTimeLocal() const
{
    if (!m_gps || !m_gps->hasTimestamp())
        return {};
    const qint64 ageMs = m_gps->timestampAgeMs();
    if (ageMs > GpsTimeMaxAgeMs)
        return {};
    // gps.timestamp is the GPSD TPV time (ISO-8601 Zulu) at the moment of fix.
    QDateTime fixUtc = QDateTime::fromString(m_gps->timestamp(), Qt::ISODate);
    if (!fixUtc.isValid())
        return {};
    // True "now" is the fix instant plus how long ago we received it (monotonic,
    // so unaffected by a wrong system clock). Render as device-local wall-clock;
    // Valhalla interprets date_time.value in the origin's baked timezone, which
    // matches the device timezone for our single-zone regions.
    const QDateTime nowLocal = fixUtc.toUTC().addMSecs(ageMs).toLocalTime();
    if (nowLocal.date().year() < 2025)  // reject an implausible GPS clock
        return {};
    return nowLocal.toString(QStringLiteral("yyyy-MM-ddTHH:mm"));
}

#include "NavigationService.h"
#include "MapService.h"
#include "routing/ValhallaClient.h"
#include "routing/RouteHelpers.h"
#include "routing/BlockedRoadSelector.h"
#include "stores/GpsStore.h"
#include "stores/NavigationStore.h"
#include "stores/VehicleStore.h"
#include "stores/SettingsStore.h"
#include "stores/SpeedLimitStore.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
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
    , m_planRpc(repo, this)
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

    connect(nav, &NavigationStore::planChanged, this, &NavigationService::onNavigationDataChanged);

    // Listen to vehicle state (for shutdown-based navigation clearing)
    connect(vehicle, &VehicleStore::stateChanged,
            this, &NavigationService::onVehicleStateChanged);

    // Listen to Valhalla URL changes
    connect(settings, &SettingsStore::valhallaUrlChanged, this, [this]() {
        resetBlockedRoad();
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

    // Auto-advance only in drive mode. A dismount cancels the countdown and
    // queues the next hop for the next unlock.
    m_hopAdvance = new QTimer(this);
    m_hopAdvance->setInterval(1000);
    connect(m_hopAdvance, &QTimer::timeout, this, [this]() {
        if (m_planState != RoutePlanState::AtStop) {
            stopHopAdvanceTimer();
            return;
        }
        if (!m_vehicle || !m_vehicle->isReadyToDrive()) {
            pausePlan();
            return;
        }
        if (m_hopMenuOpen)
            return;
        if (m_hopSecondsLeft > 0) {
            --m_hopSecondsLeft;
            emit hopPromptChanged();
            if (m_hopSecondsLeft > 0)
                return;
        }
        advanceToNextHop();
    });

    restorePlan();
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
    // Dead-reckoning keeps advancing through a GPS gap, so this is also the
    // path that releases a held route request once the estimator is usable
    // again.
    retryPendingRoute();

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

bool NavigationService::canAvoidRoad() const
{
    if (!m_route.isValid() || m_blockedLocation.isValid() || m_wasArrived)
        return false;
    return BlockedRoadSelector::ahead(m_route, selectRouteOrigin().position,
                                      m_currentSegmentIndex).isValid();
}

void NavigationService::avoidRoadAhead()
{
    if (!canAvoidRoad())
        return;
    m_blockedLocation = BlockedRoadSelector::ahead(
        m_route, selectRouteOrigin().position, m_currentSegmentIndex);
    m_valhalla->cancelPending();
    m_valhalla->setBlockedLocation(m_blockedLocation);
    emit blockedRoadChanged();
    emit positionChanged();
    requestRoute(ValhallaClient::Reason::RoadBlocked);
}

void NavigationService::clearRoadAvoidance()
{
    if (!m_blockedLocation.isValid())
        return;
    m_valhalla->cancelPending();
    resetBlockedRoad();
    if (m_destination.isValid())
        requestRoute(ValhallaClient::Reason::RoadBlocked);
}

void NavigationService::resetBlockedRoad()
{
    if (!m_blockedLocation.isValid())
        return;
    m_blockedLocation = {};
    m_valhalla->setBlockedLocation({});
    emit blockedRoadChanged();
    emit positionChanged();
}

void NavigationService::reportBlockedRoadError(const QString &message)
{
    resetBlockedRoad();
    m_errorMessage = message;
    emit errorChanged();
    m_errorLinger->start();
    setStatus(NavigationStatus::Navigating);
}

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
    if (stops.isEmpty()) {
        clearNavigation();
        return;
    }
    QJsonArray entries;
    for (const RouteStop &stop : stops) {
        if (!stop.position.isValid()) {
            raiseError(QStringLiteral("Invalid route plan stop"));
            return;
        }
        entries.append(QJsonObject{{QStringLiteral("lat"), stop.position.latitude},
                                   {QStringLiteral("lon"), stop.position.longitude},
                                   {QStringLiteral("label"), stop.label}});
    }
    if (startStep < 0 || startStep >= stops.size()) {
        raiseError(QStringLiteral("Invalid route plan step"));
        return;
    }
    requestPlan(QStringLiteral("plan.replace"),
                {{QStringLiteral("stops"), entries}, {QStringLiteral("start_step"), startStep}},
                [this, last = stops.last()]() {
        emit destinationRequested(last.position.latitude, last.position.longitude, last.label);
    });
}

void NavigationService::appendStop(double lat, double lng, const QString &label)
{
    const LatLng point{lat, lng};
    if (!point.isValid()) {
        raiseError(QStringLiteral("Invalid route plan stop"));
        return;
    }
    requestPlan(QStringLiteral("plan.append"),
                {{QStringLiteral("stop"), QJsonObject{{QStringLiteral("lat"), lat},
                    {QStringLiteral("lon"), lng}, {QStringLiteral("label"), label}}}},
                [this, lat, lng, label]() {
        emit destinationRequested(lat, lng, label);
    });
}

void NavigationService::removeStop(int index)
{
    if (index < 0 || index >= m_plan.stopCount()) return;
    requestPlan(QStringLiteral("plan.remove"),
                {{QStringLiteral("index"), index},
                 {QStringLiteral("expected_revision"), double(m_planRevision)}});
}

void NavigationService::moveStop(int from, int to)
{
    if (from < 0 || from >= m_plan.stopCount() || to < 0 || to >= m_plan.stopCount()
        || from == to) return;
    requestPlan(QStringLiteral("plan.move"),
                {{QStringLiteral("from_index"), from}, {QStringLiteral("to_index"), to},
                 {QStringLiteral("expected_revision"), double(m_planRevision)}});
}

void NavigationService::skipCurrentStop()
{
    if (!m_plan.isValid())
        return;
    advanceToNextHop();
}

void NavigationService::jumpToStop(int index)
{
    if (index < 0 || index >= m_plan.stopCount()) return;
    requestPlan(QStringLiteral("plan.jump"),
                {{QStringLiteral("index"), index},
                 {QStringLiteral("expected_revision"), double(m_planRevision)}});
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
    clearPendingRoute();
    setStatus(NavigationStatus::Idle);
    setPlanState(RoutePlanState::Held);
}

void NavigationService::keepCurrentStop()
{
    if (m_planState != RoutePlanState::AtStop || !m_plan.isValid()) return;
    requestPlan(QStringLiteral("plan.unreach"), progressArgs(), [this]() {
        stopHopAdvanceTimer();
        beginCurrentHop(ValhallaClient::Reason::Recovery);
    });
}

void NavigationService::setHopMenuOpen(bool open)
{
    if (m_hopMenuOpen == open)
        return;
    m_hopMenuOpen = open;
    if (open) {
        if (m_hopAdvance)
            m_hopAdvance->stop();
    } else if (m_planState == RoutePlanState::AtStop && m_hopSecondsLeft > 0
               && m_vehicle && m_vehicle->isReadyToDrive()) {
        m_hopAdvance->start();
    }
}

void NavigationService::pausePlan()
{
    if (m_planState != RoutePlanState::Navigating && m_planState != RoutePlanState::AtStop)
        return;

    m_pausedAfterReach = (m_planState == RoutePlanState::AtStop);

    stopHopAdvanceTimer();
    m_valhalla->cancelPending();
    clearPendingRoute();
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

    if (m_planState == RoutePlanState::None)
        beginCurrentHop(ValhallaClient::Reason::Recovery);
}

void NavigationService::requestPlan(const QString &method, const QJsonObject &payload,
                                    std::function<void()> accepted)
{
    if (method != QLatin1String("plan.get")) ++m_pendingPlanMutations;
    m_planRpc.call(method, payload, [this, method, accepted = std::move(accepted)](
                       const QJsonObject &snapshot, const QString &error) {
        if (method != QLatin1String("plan.get")) --m_pendingPlanMutations;
        if (!error.isEmpty()) {
            if (m_progressPending && method == QLatin1String("plan.reached"))
                m_progressPending = false;
            raiseError(error);
            // An owner conflict or lost reply may mean our view is stale.
            if (method == QLatin1String("plan.get")) {
                m_restorePending = false;
                return;
            }
            m_planRpc.call(QStringLiteral("plan.get"), {}, [this](const QJsonObject &fresh,
                                                                   const QString &getError) {
                if (getError.isEmpty()) applyPlanSnapshot(fresh);
                else raiseError(getError);
            });
            return;
        }
        const quint64 revision = snapshot.value(QStringLiteral("revision")).toVariant().toULongLong();
        if (revision < m_planRevision) {
            if (method == QLatin1String("plan.reached") && accepted)
                accepted();
            return;
        }
        applyPlanSnapshot(snapshot);
        if (method == QLatin1String("plan.get") && m_restorePending) {
            m_restorePending = false;
            if (m_plan.isValid() && m_planState != RoutePlanState::Complete) {
                const QString label = m_plan.currentStop().label;
                const int step = m_plan.currentStep;
                const int count = m_plan.stopCount();
                QTimer::singleShot(0, this, [this, label, step, count]() {
                    emit planRestored(label, step, count);
                });
            }
        }
        if (accepted) accepted();
    });
}

QJsonObject NavigationService::progressArgs() const
{
    if (!m_plan.isValid() || m_plan.currentStep >= m_stopIds.size()) return {};
    return {{QStringLiteral("expected_plan_id"), m_planId},
            {QStringLiteral("expected_stop_id"), m_stopIds.at(m_plan.currentStep)}};
}

void NavigationService::applyPlanSnapshot(const QJsonObject &snapshot)
{
    const QJsonValue revisionValue = snapshot.value(QStringLiteral("revision"));
    if (!revisionValue.isDouble() || !snapshot.value(QStringLiteral("stops")).isArray()
        || !snapshot.value(QStringLiteral("id")).isString()
        || !snapshot.value(QStringLiteral("current_step")).isDouble()) {
        raiseError(QStringLiteral("Invalid route plan snapshot"));
        return;
    }
    const quint64 revision = revisionValue.toVariant().toULongLong();
    if (revision < m_planRevision) return;
    const QString id = snapshot.value(QStringLiteral("id")).toString();
    const QJsonArray entries = snapshot.value(QStringLiteral("stops")).toArray();
    const int step = snapshot.value(QStringLiteral("current_step")).toInt(-1);
    if ((entries.isEmpty() && (!id.isEmpty() || step != 0))
        || (!entries.isEmpty() && (id.isEmpty() || step < 0 || step >= entries.size()))) {
        raiseError(QStringLiteral("Invalid route plan snapshot"));
        return;
    }
    RoutePlan plan;
    QStringList ids;
    for (const QJsonValue &entry : entries) {
        const QJsonObject item = entry.toObject();
        const QString stopId = item.value(QStringLiteral("id")).toString();
        RouteStop stop;
        stop.position = {item.value(QStringLiteral("lat")).toDouble(),
                         item.value(QStringLiteral("lon")).toDouble()};
        if (stopId.isEmpty() || !stop.position.isValid() || ids.contains(stopId)) {
            raiseError(QStringLiteral("Invalid route plan snapshot"));
            return;
        }
        ids.append(stopId);
        if (!m_numericStopIds.contains(stopId))
            m_numericStopIds.insert(stopId, m_nextStopId++);
        stop.id = m_numericStopIds.value(stopId);
        stop.label = item.value(QStringLiteral("label")).toString();
        stop.reached = item.value(QStringLiteral("reached")).toBool();
        plan.stops.append(stop);
    }
    plan.currentStep = step;
    plan.keepCurrentStop = snapshot.value(QStringLiteral("keep_current_stop")).toBool();
    const QString oldTarget = m_plan.isValid() && m_plan.currentStep < m_stopIds.size()
        ? m_stopIds.at(m_plan.currentStep) : QString();
    const bool targetChanged = id != m_planId || (plan.isValid() && ids.at(step) != oldTarget);
    const bool becameReached = plan.isValid() && !targetChanged
        && !m_plan.currentStop().reached && plan.currentStop().reached;
    const bool stopsChanged = plan.stops != m_plan.stops;
    const bool reopened = plan.isValid() && !targetChanged && m_planState == RoutePlanState::Complete
        && !plan.atLastStop() && plan.currentStop().reached;
    if (revision == m_planRevision && id == m_planId && plan.stops == m_plan.stops
        && plan.currentStep == m_plan.currentStep
        && plan.keepCurrentStop == m_plan.keepCurrentStop) return;
    m_planRevision = revision;
    m_planId = id;
    m_stopIds = ids;
    if (!plan.isValid()) {
        clearLocalNavigation();
        return;
    }
    m_plan = plan;
    emit planChanged();
    if (targetChanged) {
        resetBlockedRoad();
        stopHopAdvanceTimer();
        m_restoreReachedAwaitingVehicleState = false;
        if (plan.currentStop().reached) {
            m_pausedAfterReach = true;
            m_valhalla->cancelPending();
            m_valhalla->cancelPreview();
            clearPendingRoute();
            m_route = Route();
            m_wasArrived = false;
            emit arrivalReset();
            emit routeChanged();
            m_destination = plan.currentStop().position;
            m_destAddress = plan.currentStop().label;
            emit destinationChanged();
            if (plan.atLastStop()) {
                setPlanState(RoutePlanState::Complete);
                setStatus(NavigationStatus::Idle);
            } else if (m_vehicle && m_vehicle->isReadyToDrive()) showHopReached();
            else {
                m_restoreReachedAwaitingVehicleState = m_vehicle
                    && m_vehicle->state() == static_cast<int>(ScootEnums::VehicleState::Unknown);
                setPlanState(RoutePlanState::Paused);
            }
        } else if (m_vehicle && m_vehicle->isReadyToDrive()) {
            beginCurrentHop(ValhallaClient::Reason::Destination);
        } else {
            m_valhalla->cancelPending();
            m_valhalla->cancelPreview();
            clearPendingRoute();
            m_route = Route();
            m_wasArrived = false;
            emit arrivalReset();
            emit routeChanged();
            m_destination = plan.currentStop().position;
            m_destAddress = plan.currentStop().label;
            emit destinationChanged();
            setStatus(NavigationStatus::Idle);
            setPlanState(RoutePlanState::Paused);
        }
    } else if ((becameReached && !m_progressPending) || reopened) {
        showHopReached();
    } else if (!plan.currentStop().reached && m_planState == RoutePlanState::Complete
               && m_vehicle && m_vehicle->isReadyToDrive()) {
        beginCurrentHop(ValhallaClient::Reason::Destination);
    } else if (stopsChanged) {
        refreshPlanOverview();
    }
}

void NavigationService::beginCurrentHop(ValhallaClient::Reason reason)
{
    if (!m_plan.isValid())
        return;

    // Tear down any prior hop/route state before starting the next one. Without
    // this the old polyline, arrival pill, and stale distances linger on the
    // map until the new route comes back from the router.
    m_valhalla->cancelPending();
    resetBlockedRoad();
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
    emit destinationChanged();
    refreshPlanOverview();

    if (!selectRouteOrigin().isValid()) {
        // No position we can route from yet. Hold the request and show the
        // waiting state; retryPendingRoute() re-issues it as soon as the fix
        // or the estimator is usable again.
        deferRouteForPosition(reason);
        return;
    }
    clearPendingRoute();
    setStatus(NavigationStatus::Calculating);
    requestRoute(reason);
}

void NavigationService::onHopReached()
{
    if (!m_plan.isValid() || m_progressPending || m_plan.currentStop().reached) return;
    m_progressPending = true;
    const QString expectedPlanId = m_planId;
    const QString expectedStopId = m_stopIds.value(m_plan.currentStep);
    requestPlan(QStringLiteral("plan.reached"), progressArgs(),
                [this, expectedPlanId, expectedStopId]() {
        m_progressPending = false;
        if (m_planId == expectedPlanId && m_stopIds.value(m_plan.currentStep) == expectedStopId
            && m_plan.isValid() && m_plan.currentStop().reached)
            showHopReached();
    });
}

void NavigationService::showHopReached()
{
    if (!m_plan.isValid()) return;
    RouteInstruction arrival;
    arrival.type = ManeuverType::Arrive;
    for (auto it = m_route.instructions.crbegin(); it != m_route.instructions.crend(); ++it) {
        if (it->type == ManeuverType::Arrive || it->type == ManeuverType::ArriveLeft
            || it->type == ManeuverType::ArriveRight) { arrival = *it; break; }
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
    if (m_plan.atLastStop()) {
        setPlanState(RoutePlanState::Complete);
        emit arrived();
        return;
    }
    setPlanState(RoutePlanState::AtStop);
    emit hopReached(m_plan.currentStep, m_plan.currentStop().label);
    startHopAdvanceTimer();
}

void NavigationService::advanceToNextHop()
{
    if (!m_plan.isValid()) return;
    stopHopAdvanceTimer();
    if (m_plan.atLastStop()) {
        clearNavigation();
        return;
    }
    requestPlan(QStringLiteral("plan.advance"), progressArgs());
}

void NavigationService::completePlan()
{
    clearNavigation();
}

void NavigationService::startHopAdvanceTimer()
{
    m_hopSecondsLeft = HopAdvanceTimeoutSeconds;
    emit hopPromptChanged();
    if (m_vehicle && m_vehicle->isReadyToDrive()) {
        if (!m_hopMenuOpen)
            m_hopAdvance->start();
    } else {
        pausePlan();
    }
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
    requestPlan(QStringLiteral("plan.get"), {});
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
    if (m_planId.isEmpty()) {
        if (m_restorePending || m_pendingPlanMutations > 0) {
            raiseError(QStringLiteral("Route plan is still loading"));
            return;
        }
        clearLocalNavigation();
        return;
    }
    requestPlan(QStringLiteral("plan.clear"),
                {{QStringLiteral("expected_plan_id"), m_planId}});
}

void NavigationService::clearLocalNavigation()
{
    if (!m_plan.isValid() && !m_destination.isValid() && m_status == NavigationStatus::Idle)
        return;
    const bool wasArrived = m_wasArrived;
    m_valhalla->cancelPending();
    resetBlockedRoad();
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
    clearPendingRoute();
    m_navigationCadence.reset();
    m_wasArrived = false;
    m_pausedAfterReach = false;
    m_progressPending = false;
    emit arrivalReset();
    m_currentSegmentIndex = 0;
    m_hasLastPassedManeuver = false;
    m_prevLeadingShapeIdx = -1;
    m_plan = RoutePlan();
    m_valhalla->cancelPreview();
    clearPlanOverview();
    setStatus(NavigationStatus::Idle);
    clearError();
    emit routeChanged();
    emit destinationChanged();
    emit instructionChanged();
    emit positionChanged();
    updateRoundaboutRender();
    emit planChanged();
    setPlanState(RoutePlanState::None);
    if (!wasArrived) emit navigationStopped();
}

void NavigationService::setRoute(const Route &route)
{
    // An injected route supersedes any real request still queued, so a late
    // dispatch cannot clobber it (simulator and tests inject routes).
    m_valhalla->cancelPending();
    resetBlockedRoad();
    clearPendingRoute();
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

    // Release a route request that was held for lack of a usable origin.
    // This is the 1 Hz GPS path; onVehiclePositionChanged covers the DR ticks.
    retryPendingRoute();

    // Recovery: destination loaded but route not yet calculated (GPS wasn't
    // ready). Never runs for a held, paused, or finished plan: those keep their
    // route or deliberately have none, and must not silently resume. A request
    // already held by retryPendingRoute() is left to that path.
    const bool planGuiding = m_planState == RoutePlanState::None
                          || m_planState == RoutePlanState::Planning
                          || m_planState == RoutePlanState::Navigating;
    if (!m_pendingRoute && planGuiding && m_destination.isValid() && !m_route.isValid()
        && (m_status == NavigationStatus::Idle || m_status == NavigationStatus::Error
            || m_status == NavigationStatus::WaitingForPosition)) {
        if (selectRouteOrigin().isValid()) {
            LatLng pos = currentPosition();
            double dist = pos.distanceTo(m_destination);
            if (dist < ArrivalProximity && !m_plan.keepCurrentStop) {
                if (m_plan.isValid())
                    onHopReached();
                else
                    clearNavigation();
            } else {
                requestRoute(ValhallaClient::Reason::Recovery);
            }
        } else {
            deferRouteForPosition(ValhallaClient::Reason::Recovery);
        }
    }
}

void NavigationService::onNavigationDataChanged()
{
    if (!m_nav) return;
    const QJsonDocument document = QJsonDocument::fromJson(m_nav->plan().toUtf8());
    if (document.isObject()) applyPlanSnapshot(document.object());
}

void NavigationService::onVehicleStateChanged()
{
    if (!m_vehicle) return;

    if (m_vehicle->isReadyToDrive()) {
        if (m_restoreReachedAwaitingVehicleState) {
            m_restoreReachedAwaitingVehicleState = false;
            showHopReached();
        } else if (m_planState == RoutePlanState::Paused) {
            resumePlan();
        }
        return;
    }

    // Leaving the scooter (full lock, standby, or hop-on) ends a single trip.
    // HopOnLearning is excluded: the rider is still present, teaching the
    // scooter the combo.
    bool isLeaving = m_vehicle->isShuttingDown() || m_vehicle->isStandBy()
                  || m_vehicle->hopOnActive()
                  || m_vehicle->state() == static_cast<int>(ScootEnums::VehicleState::Parked);
    if (!isLeaving)
        return;
    m_restoreReachedAwaitingVehicleState = false;
    if (m_plan.isValid() && m_plan.keepCurrentStop) {
        requestPlan(QStringLiteral("plan.set-keep-current"),
                    {{QStringLiteral("expected_plan_id"), m_planId},
                     {QStringLiteral("expected_stop_id"), m_stopIds.value(m_plan.currentStep)},
                     {QStringLiteral("keep"), false}});
    }

    if (m_plan.isValid() && !m_plan.atLastStop()
        && m_planState == RoutePlanState::Navigating
        && selectRouteOrigin().isValid()
        && currentPosition().distanceTo(m_plan.currentStop().position) < ArrivalProximity) {
        onHopReached();
    }

    // Only a full shutdown ends a request that never produced a route, and
    // only for a single destination. A multi-hop plan is a persisted trip:
    // pause it so the remaining hops survive even when the current hop has no
    // route yet.
    if (m_vehicle->isShuttingDown() && !m_route.isValid()) {
        if (m_plan.isValid() && m_plan.stopCount() > 1) {
            pausePlan();
            return;
        }
        if (m_destination.isValid() || m_plan.isValid() || m_pendingRoute) {
            qDebug() << "NavigationService: clearing navigation (shutting down with no established route)";
            clearNavigation();
        }
        return;
    }

    // Dismounting at a reached intermediate stop queues the next hop for unlock.
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
    clearPendingRoute();
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
    if (m_activeRouteReason == ValhallaClient::Reason::RoadBlocked && m_route.isValid()) {
        reportBlockedRoadError(QStringLiteral("No alternative route; existing route unchanged: %1").arg(error));
        return;
    }
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
        reason == ValhallaClient::Reason::LanguageChange ||
        reason == ValhallaClient::Reason::RoadBlocked;

    if (userReason) {
        if (reason == ValhallaClient::Reason::RoadBlocked && m_route.isValid()) {
            reportBlockedRoadError(QStringLiteral("No alternative route; existing route unchanged"));
            return;
        }
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
    if (straightLineToDestination < ArrivalProximity && !m_plan.keepCurrentStop) {
        if (m_plan.isValid()) {
            onHopReached();
            return;
        }
        m_wasArrived = true;
        setStatus(NavigationStatus::Arrived);
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
    if (!origin.isValid()) {
        deferRouteForPosition(reason);
        return false;
    }
    clearPendingRoute();
    m_valhalla->requestRoute(origin, m_destination, reason);
    return true;
}

void NavigationService::deferRouteForPosition(ValhallaClient::Reason reason)
{
    m_pendingRoute = true;
    m_pendingRouteReason = reason;
    m_pendingRouteHopId = m_plan.isValid() ? m_plan.currentStop().id : -1;

    // Only the no-route case needs the dedicated waiting state. When a route is
    // already on screen (a preference/language recalculation), keep showing it
    // and let the retry refresh it in the background.
    if (!m_route.isValid()) {
        qDebug() << "NavigationService: holding route request for a trustworthy position";
        setStatus(NavigationStatus::WaitingForPosition);
    }
}

void NavigationService::retryPendingRoute()
{
    if (!m_pendingRoute)
        return;

    // A held request belongs to the hop it was issued for. A paused, held, or
    // finished plan, a different hop having become current, or a cleared
    // destination all supersede it.
    const bool hopSuperseded = m_plan.isValid()
        && m_plan.currentStop().id != m_pendingRouteHopId;
    const bool planGuiding = m_planState == RoutePlanState::None
                          || m_planState == RoutePlanState::Planning
                          || m_planState == RoutePlanState::Navigating;
    if (hopSuperseded || !planGuiding || !m_destination.isValid()) {
        clearPendingRoute();
        return;
    }

    if (!selectRouteOrigin().isValid())
        return;

    const ValhallaClient::Reason reason = m_pendingRouteReason;
    if (!requestRoute(reason)) {
        // Still not dispatchable (e.g. destination cleared by the caller).
        return;
    }
    if (!m_route.isValid())
        setStatus(NavigationStatus::Calculating);
}

void NavigationService::clearPendingRoute()
{
    m_pendingRoute = false;
    m_pendingRouteHopId = -1;
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

#include "SimulatorService.h"
#include "repositories/MdbRepository.h"
#include "services/NavigationService.h"
#include "routing/RouteHelpers.h"
#include "core/AppConfig.h"

#include <QtMath>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

SimulatorService::SimulatorService(MdbRepository *repo, NavigationService *nav, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_nav(nav)
{
    m_autoDriveTimer = new QTimer(this);
    m_autoDriveTimer->setInterval(100); // 10 Hz
    connect(m_autoDriveTimer, &QTimer::timeout, this, &SimulatorService::autoDriveTick);

    // Keep GPS timestamp fresh so hasRecentFix stays true while parked
    m_gpsTimestampTimer = new QTimer(this);
    m_gpsTimestampTimer->setInterval(10000); // every 10s
    connect(m_gpsTimestampTimer, &QTimer::timeout, this, [this]() {
        if (!m_autoDriveActive) {
            const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            m_repo->set(QStringLiteral("gps"), QStringLiteral("timestamp"), now);
            m_repo->set(QStringLiteral("gps"), QStringLiteral("updated"), now);
        }
    });
    m_gpsTimestampTimer->start();

    applyDefaults();
}

// --- Vehicle ---

void SimulatorService::setVehicleState(const QString &state)
{
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("state"), state);
}

void SimulatorService::setKickstand(const QString &state)
{
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("kickstand"), state);
    // Real vehicle-service resets the auto-standby timer on every kickstand
    // change (system.go handleInputChange "kickstand" branch).
    resetAutoStandbyTimerIfActive();
}

void SimulatorService::setBlinkerState(const QString &state)
{
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("blinker:state"), state);
}

void SimulatorService::setBrakeLeft(bool pressed)
{
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("brake:left"),
                pressed ? QStringLiteral("on") : QStringLiteral("off"));
    m_repo->publishButtonEvent(QStringLiteral("brake:left:") + (pressed ? QStringLiteral("on") : QStringLiteral("off")));
    // Real vehicle-service resets the auto-standby timer whenever either brake
    // is pressed (system.go handleInputChange brake branch).
    if (pressed)
        resetAutoStandbyTimerIfActive();
}

void SimulatorService::setBrakeRight(bool pressed)
{
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("brake:right"),
                pressed ? QStringLiteral("on") : QStringLiteral("off"));
    m_repo->publishButtonEvent(QStringLiteral("brake:right:") + (pressed ? QStringLiteral("on") : QStringLiteral("off")));
    if (pressed)
        resetAutoStandbyTimerIfActive();
}

void SimulatorService::setSeatboxButton(bool pressed)
{
    m_repo->publishButtonEvent(QStringLiteral("seatbox:") + (pressed ? QStringLiteral("on") : QStringLiteral("off")));
    // Real vehicle-service resets the auto-standby timer on seatbox button
    // press (only on press, not release — matches system.go behavior).
    if (pressed)
        resetAutoStandbyTimerIfActive();
}

void SimulatorService::simulateBrakeTap(const QString &side)
{
    if (side == QLatin1String("left")) {
        setBrakeLeft(true);
        QTimer::singleShot(200, this, [this]() { setBrakeLeft(false); });
    } else {
        setBrakeRight(true);
        QTimer::singleShot(200, this, [this]() { setBrakeRight(false); });
    }
}

void SimulatorService::simulateBrakeHold(const QString &side, int durationMs)
{
    if (side == QLatin1String("left")) {
        setBrakeLeft(true);
        QTimer::singleShot(durationMs, this, [this]() { setBrakeLeft(false); });
    } else {
        setBrakeRight(true);
        QTimer::singleShot(durationMs, this, [this]() { setBrakeRight(false); });
    }
}

void SimulatorService::simulateBrakeDoubleTap(const QString &side)
{
    simulateBrakeTap(side);
    QTimer::singleShot(400, this, [this, side]() { simulateBrakeTap(side); });
}

void SimulatorService::simulateSeatboxTap()
{
    setSeatboxButton(true);
    QTimer::singleShot(200, this, [this]() { setSeatboxButton(false); });
}

void SimulatorService::simulateSeatboxHold(int durationMs)
{
    setSeatboxButton(true);
    QTimer::singleShot(durationMs, this, [this]() { setSeatboxButton(false); });
}

void SimulatorService::simulateSeatboxDoubleTap()
{
    simulateSeatboxTap();
    QTimer::singleShot(400, this, [this]() { simulateSeatboxTap(); });
}

void SimulatorService::setSeatboxLock(const QString &state)
{
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("seatbox:lock"), state);
}

void SimulatorService::setHandlebarLock(const QString &state)
{
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("handlebar:lock-sensor"), state);
}

void SimulatorService::setHornButton(bool pressed)
{
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("horn-button"),
                pressed ? QStringLiteral("on") : QStringLiteral("off"));
}

// --- Engine ---

void SimulatorService::setSpeed(double speed)
{
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("speed"),
                QString::number(speed, 'f', 1));
}

void SimulatorService::setOdometer(double km)
{
    m_odometer = km;
    // Store in meters (matching real ECU which reports meters)
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("odometer"),
                QString::number(km * 1000, 'f', 0));
}

void SimulatorService::setEngineTemperature(double temp)
{
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("temperature"),
                QString::number(temp, 'f', 1));
}

void SimulatorService::setMotorPower(bool on)
{
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("state"),
                on ? QStringLiteral("on") : QStringLiteral("off"));
}

void SimulatorService::setMotorCurrent(double milliamps)
{
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("motor:current"),
                QString::number(milliamps, 'f', 0));
}

void SimulatorService::setMotorVoltage(double millivolts)
{
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("motor:voltage"),
                QString::number(millivolts, 'f', 0));
}

void SimulatorService::setThrottle(bool on)
{
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("throttle"),
                on ? QStringLiteral("on") : QStringLiteral("off"));
}

// --- Battery ---

void SimulatorService::setBatteryField(int slot, const QString &field, const QString &value)
{
    const QString channel = QStringLiteral("battery:") + QString::number(slot);
    m_repo->set(channel, field, value);
}

void SimulatorService::setBatteryCharge(int slot, int percent)
{
    if (slot == 0) m_batteryCharge0 = percent;
    else m_batteryCharge1 = percent;
    setBatteryField(slot, QStringLiteral("charge"), QString::number(percent));
    // Derive voltage from charge (roughly 42V empty to 58.8V full for a 48V pack)
    int mv = 42000 + (percent * 168); // 42000..58800 mV
    setBatteryField(slot, QStringLiteral("voltage"), QString::number(mv));
}

void SimulatorService::setBatteryPresent(int slot, bool present)
{
    setBatteryField(slot, QStringLiteral("present"),
                    present ? QStringLiteral("true") : QStringLiteral("false"));
}

void SimulatorService::setBatteryState(int slot, const QString &state)
{
    setBatteryField(slot, QStringLiteral("state"), state);
}

void SimulatorService::setBatteryVoltage(int slot, int millivolts)
{
    setBatteryField(slot, QStringLiteral("voltage"), QString::number(millivolts));
}

void SimulatorService::setBatteryCurrent(int slot, int milliamps)
{
    setBatteryField(slot, QStringLiteral("current"), QString::number(milliamps));
}

void SimulatorService::setBatteryTemperature(int slot, int temp)
{
    setBatteryField(slot, QStringLiteral("temperature:0"), QString::number(temp));
    setBatteryField(slot, QStringLiteral("temperature:1"), QString::number(temp));
    setBatteryField(slot, QStringLiteral("temperature:2"), QString::number(temp + 1));
    setBatteryField(slot, QStringLiteral("temperature:3"), QString::number(temp + 1));
}

// --- GPS ---

void SimulatorService::setGpsPosition(double lat, double lng)
{
    m_autoDriveLat = lat;
    m_autoDriveLng = lng;
    if (m_gpsFrozen) return;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_repo->set(QStringLiteral("gps"), QStringLiteral("latitude"),
                QString::number(lat, 'f', 8));
    m_repo->set(QStringLiteral("gps"), QStringLiteral("longitude"),
                QString::number(lng, 'f', 8));
    m_repo->set(QStringLiteral("gps"), QStringLiteral("updated"), now);
    m_repo->set(QStringLiteral("gps"), QStringLiteral("timestamp"), now);
}

void SimulatorService::setGpsCourse(double course)
{
    m_autoDriveBearing = course;
    m_repo->set(QStringLiteral("gps"), QStringLiteral("course"),
                QString::number(course, 'f', 1));
}

void SimulatorService::setGpsSpeed(double speed)
{
    m_repo->set(QStringLiteral("gps"), QStringLiteral("speed"),
                QString::number(speed, 'f', 1));
}

void SimulatorService::setGpsState(const QString &state)
{
    m_repo->set(QStringLiteral("gps"), QStringLiteral("state"), state);
}

// --- Internet ---

void SimulatorService::setModemState(const QString &state)
{
    m_repo->set(QStringLiteral("internet"), QStringLiteral("modem-state"), state);
}

void SimulatorService::setSignalQuality(int quality)
{
    m_repo->set(QStringLiteral("internet"), QStringLiteral("signal-quality"),
                QString::number(quality));
}

void SimulatorService::setAccessTech(const QString &tech)
{
    m_repo->set(QStringLiteral("internet"), QStringLiteral("access-tech"), tech);
}

void SimulatorService::setCloudConnection(const QString &state)
{
    m_repo->set(QStringLiteral("internet"), QStringLiteral("unu-cloud"), state);
    m_repo->set(QStringLiteral("internet"), QStringLiteral("status"), state);
}

// --- Bluetooth ---

void SimulatorService::setBluetoothStatus(const QString &state)
{
    m_repo->set(QStringLiteral("ble"), QStringLiteral("status"), state);
    m_repo->set(QStringLiteral("ble"), QStringLiteral("service-health"), QStringLiteral("ok"));
}

// --- USB / UMS ---

void SimulatorService::setUsbStatus(const QString &status)
{
    m_repo->set(QStringLiteral("usb"), QStringLiteral("status"), status);
}

void SimulatorService::setUsbMode(const QString &mode)
{
    m_repo->set(QStringLiteral("usb"), QStringLiteral("mode"), mode);
}

// --- Speed limit ---

void SimulatorService::setSpeedLimit(const QString &limit)
{
    m_repo->set(QStringLiteral("speed-limit"), QStringLiteral("speed-limit"), limit);
}

void SimulatorService::setRoadName(const QString &name)
{
    m_repo->set(QStringLiteral("speed-limit"), QStringLiteral("road-name"), name);
}

void SimulatorService::setRoadType(const QString &type)
{
    m_repo->set(QStringLiteral("speed-limit"), QStringLiteral("road-type"), type);
}

// --- Settings ---

void SimulatorService::setTheme(const QString &theme)
{
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.theme"), theme);
}

void SimulatorService::setLanguage(const QString &lang)
{
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.language"), lang);
}

void SimulatorService::setDualBattery(bool enabled)
{
    m_repo->set(QStringLiteral("settings"), QStringLiteral("scooter.dual-battery"),
                enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void SimulatorService::setTrafficOverlay(bool enabled)
{
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.map.traffic-overlay"),
                enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

// --- Auto-lock countdown ---

void SimulatorService::setAutoStandbyDeadline(int secondsFromNow)
{
    // Mirror real vehicle-service: the deadline is only meaningful while
    // parked, so force parked state too. This makes the simulator buttons
    // "just work" regardless of the previously selected state.
    setVehicleState(QStringLiteral("parked"));
    const qint64 deadline = QDateTime::currentSecsSinceEpoch() + secondsFromNow;
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("auto-standby-deadline"),
                QString::number(deadline));
    m_autoStandbyActive = true;
}

void SimulatorService::clearAutoStandbyDeadline()
{
    // Empty value is treated as "no timer active" by AutoStandbyStore. We
    // don't HDEL because the InMemory simulator repo doesn't notify
    // SyncableStore on hdel — set("") goes through the fieldsUpdated path.
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("auto-standby-deadline"),
                QString());
    m_autoStandbyActive = false;
}

void SimulatorService::setAutoStandbySetting(int seconds)
{
    m_autoStandbySeconds = seconds;
    m_repo->set(QStringLiteral("settings"), QStringLiteral("scooter.auto-standby-seconds"),
                QString::number(seconds));
}

void SimulatorService::resetAutoStandbyTimerIfActive()
{
    if (!m_autoStandbyActive)
        return;
    const qint64 deadline = QDateTime::currentSecsSinceEpoch() + m_autoStandbySeconds;
    m_repo->set(QStringLiteral("vehicle"), QStringLiteral("auto-standby-deadline"),
                QString::number(deadline));
}

// --- Presets ---

void SimulatorService::loadPreset(const QString &name)
{
    if (name == QLatin1String("parked")) {
        setVehicleState(QStringLiteral("parked"));
        setKickstand(QStringLiteral("down"));
        setHandlebarLock(QStringLiteral("unlocked"));
        setMotorPower(true);
        setSpeed(0);
        setThrottle(false);
        setEngineTemperature(25);
        setBatteryPresent(0, true);
        setBatteryState(0, QStringLiteral("active"));
        setBatteryCharge(0, 80);
        setBatteryTemperature(0, 25);
        setBatteryPresent(1, true);
        setBatteryState(1, QStringLiteral("idle"));
        setBatteryCharge(1, 75);
        setBatteryTemperature(1, 24);
        setGpsState(QStringLiteral("fix-established"));
        setGpsPosition(52.520008, 13.404954);
        setGpsCourse(0);
        setGpsSpeed(0);
        setModemState(QStringLiteral("connected"));
        setSignalQuality(75);
        setAccessTech(QStringLiteral("LTE"));
        setCloudConnection(QStringLiteral("connected"));
        setBluetoothStatus(QStringLiteral("connected"));
        setSpeedLimit(QStringLiteral("30"));
        setRoadName(QStringLiteral("Alexanderplatz"));
        setRoadType(QStringLiteral("secondary"));
        setBlinkerState(QStringLiteral("off"));
        setBrakeLeft(false);
        setBrakeRight(false);
        stopAutoDrive();
        m_repo->set(QStringLiteral("cb-battery"), QStringLiteral("present"), QStringLiteral("true"));
        m_repo->set(QStringLiteral("cb-battery"), QStringLiteral("charge"), QStringLiteral("95"));
        m_repo->set(QStringLiteral("cb-battery"), QStringLiteral("temperature"), QStringLiteral("23"));
        // Use online routing in simulator (no local valhalla)
        m_repo->set(QStringLiteral("dashboard"), QStringLiteral("dashboard.valhalla-url"),
                    QLatin1String(AppConfig::valhallaOnlineEndpoint));
    } else if (name == QLatin1String("ready")) {
        loadPreset(QStringLiteral("parked"));
        setVehicleState(QStringLiteral("ready-to-drive"));
        setKickstand(QStringLiteral("up"));
    } else if (name == QLatin1String("driving")) {
        loadPreset(QStringLiteral("ready"));
        startAutoDrive(25);
    } else if (name == QLatin1String("driving-fast")) {
        loadPreset(QStringLiteral("ready"));
        startAutoDrive(45);
    } else if (name == QLatin1String("low-battery")) {
        loadPreset(QStringLiteral("parked"));
        setBatteryCharge(0, 8);
        setBatteryCharge(1, 5);
    } else if (name == QLatin1String("charging")) {
        loadPreset(QStringLiteral("parked"));
        setVehicleState(QStringLiteral("stand-by"));
        setMotorPower(false);
        setBatteryState(0, QStringLiteral("active"));
        setBatteryCurrent(0, 3500);
        setBatteryCharge(0, 45);
        setBatteryState(1, QStringLiteral("active"));
        setBatteryCurrent(1, 3500);
        setBatteryCharge(1, 42);
    } else if (name == QLatin1String("no-gps")) {
        loadPreset(QStringLiteral("parked"));
        setGpsState(QStringLiteral("searching"));
    } else if (name == QLatin1String("offline")) {
        loadPreset(QStringLiteral("parked"));
        setModemState(QStringLiteral("disconnected"));
        setCloudConnection(QStringLiteral("disconnected"));
        setSignalQuality(0);
    } else if (name == QLatin1String("single-battery")) {
        loadPreset(QStringLiteral("parked"));
        setBatteryPresent(1, false);
        setDualBattery(false);
    } else if (name == QLatin1String("off")) {
        setVehicleState(QStringLiteral("off"));
        setMotorPower(false);
        setSpeed(0);
        stopAutoDrive();
    }

    qDebug() << "Simulator: loaded preset" << name;
}

// --- Routes ---

void SimulatorService::loadTestRoute(int index)
{
    QString path = QStringLiteral(":/ScootUI/assets/routes/route") + QString::number(index) + QStringLiteral(".json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Simulator: Failed to open route file" << path;
        return;
    }

    QByteArray data = file.readAll();
    Route route = RouteHelpers::parseRouteResponse(data);
    if (route.isValid()) {
        qDebug() << "Simulator: Loaded route" << index << "with" << route.waypoints.size() << "waypoints";

        // Store route for auto-drive waypoint following
        m_route = route;
        m_routeWaypointIndex = 0;
        m_currentInstructionIndex = 0;

        // Move vehicle to route start
        const auto &start = route.waypoints.first();
        setGpsPosition(start.latitude, start.longitude);
        setGpsState(QStringLiteral("fix-established"));

        // Calculate initial bearing toward second waypoint
        if (route.waypoints.size() > 1) {
            const auto &next = route.waypoints[1];
            m_autoDriveBearing = start.bearingTo(next);
            setGpsCourse(m_autoDriveBearing);
        }

        // Set initial road info from first instruction
        if (!route.instructions.isEmpty()) {
            const auto &first = route.instructions.first();
            if (!first.streetName.isEmpty())
                setRoadName(first.streetName);
            setSpeedLimit(QStringLiteral("50"));
            setRoadType(QStringLiteral("secondary"));
        }

        // Set destination in navigation store so the full nav UI activates
        const auto &dest = route.waypoints.last();
        m_repo->set(QStringLiteral("navigation"), QStringLiteral("latitude"),
                    QString::number(dest.latitude, 'f', 8));
        m_repo->set(QStringLiteral("navigation"), QStringLiteral("longitude"),
                    QString::number(dest.longitude, 'f', 8));
        QString address;
        if (index == 3)
            address = QStringLiteral("Boxhagener Str. 105, Friedrichshain");
        else
            address = QStringLiteral("Invalidenstraße, Moabit");
        m_repo->set(QStringLiteral("navigation"), QStringLiteral("address"), address);

        // Delay setRoute so GPS store picks up the position we just wrote
        QTimer::singleShot(100, this, [this, route]() {
            m_nav->setRoute(route);
        });
    } else {
        qWarning() << "Simulator: Failed to parse route" << index;
    }
}

// --- Auto-drive ---

void SimulatorService::startAutoDrive(double targetSpeed)
{
    m_autoDriveTargetSpeed = targetSpeed;
    if (!m_autoDriveActive) {
        m_autoDriveActive = true;
        emit autoDriveActiveChanged();
    }
    setVehicleState(QStringLiteral("ready-to-drive"));
    setKickstand(QStringLiteral("up"));
    setMotorPower(true);
    setThrottle(true);
    m_autoDriveTimer->start();
}

void SimulatorService::stopAutoDrive()
{
    m_autoDriveTimer->stop();
    if (m_autoDriveActive) {
        m_autoDriveActive = false;
        emit autoDriveActiveChanged();
    }
    m_autoDriveSpeed = 0;
    m_route = Route();
    m_routeWaypointIndex = 0;
    m_currentInstructionIndex = 0;
    emit autoDriveSpeedChanged();
    setSpeed(0);
    setGpsSpeed(0);
    setThrottle(false);
}

void SimulatorService::autoDriveTick()
{
    // Smooth acceleration/deceleration
    const double accel = 0.5; // km/h per tick (5 km/h/s)
    if (m_autoDriveSpeed < m_autoDriveTargetSpeed)
        m_autoDriveSpeed = qMin(m_autoDriveSpeed + accel, m_autoDriveTargetSpeed);
    else if (m_autoDriveSpeed > m_autoDriveTargetSpeed)
        m_autoDriveSpeed = qMax(m_autoDriveSpeed - accel, m_autoDriveTargetSpeed);

    emit autoDriveSpeedChanged();
    setSpeed(m_autoDriveSpeed);
    setGpsSpeed(m_autoDriveSpeed);

    const double dt = 0.1; // seconds per tick
    const double speedMs = m_autoDriveSpeed / 3.6;
    double distRemaining = speedMs * dt; // meters to travel this tick

    // Follow route waypoints if a route is loaded
    if (m_route.isValid() && m_routeWaypointIndex < m_route.waypoints.size() - 1) {
        while (distRemaining > 0 && m_routeWaypointIndex < m_route.waypoints.size() - 1) {
            const LatLng current = {m_autoDriveLat, m_autoDriveLng};
            const LatLng &target = m_route.waypoints[m_routeWaypointIndex + 1];
            double distToTarget = current.distanceTo(target);

            // Update bearing toward next waypoint
            m_autoDriveBearing = current.bearingTo(target);

            if (distRemaining >= distToTarget) {
                // Reached this waypoint, advance to next
                m_autoDriveLat = target.latitude;
                m_autoDriveLng = target.longitude;
                distRemaining -= distToTarget;
                m_routeWaypointIndex++;
            } else {
                // Move partially toward the next waypoint
                double fraction = distRemaining / distToTarget;
                m_autoDriveLat += (target.latitude - current.latitude) * fraction;
                m_autoDriveLng += (target.longitude - current.longitude) * fraction;
                distRemaining = 0;
            }
        }

        // Stop when we reach the end of the route
        if (m_routeWaypointIndex >= m_route.waypoints.size() - 1) {
            qDebug() << "Simulator: reached end of route";
            m_autoDriveTargetSpeed = 0;
        }
    } else {
        // No route: move along bearing with gentle curve (fallback)
        const double distKm = speedMs * dt / 1000.0;
        const double bearingRad = qDegreesToRadians(m_autoDriveBearing);
        m_autoDriveLat += (distKm * qCos(bearingRad)) / 111.32;
        m_autoDriveLng += (distKm * qSin(bearingRad)) / (111.32 * qCos(qDegreesToRadians(m_autoDriveLat)));
        m_autoDriveBearing = fmod(m_autoDriveBearing + 0.1, 360.0);
    }

    setGpsPosition(m_autoDriveLat, m_autoDriveLng);
    setGpsCourse(m_autoDriveBearing);

    // Update road name / speed limit from route instructions
    if (m_route.isValid()) {
        updateRoadInfo();
    }

    // Slowly drain battery
    const double distKm = speedMs * dt / 1000.0;
    m_batteryCharge0 = qMax(0.0, m_batteryCharge0 - 0.001);
    m_batteryCharge1 = qMax(0.0, m_batteryCharge1 - 0.001);
    if (static_cast<int>(m_batteryCharge0 * 10) % 10 == 0) {
        setBatteryCharge(0, static_cast<int>(m_batteryCharge0));
        setBatteryCharge(1, static_cast<int>(m_batteryCharge1));
    }

    // Update odometer
    m_odometer += distKm;
    setOdometer(m_odometer);

    // Simulate motor current based on speed
    int current = static_cast<int>(m_autoDriveSpeed * 200); // rough mA
    setBatteryCurrent(0, -current / 2);
    setBatteryCurrent(1, -current / 2);
}

void SimulatorService::updateRoadInfo()
{
    // Find the current instruction based on waypoint index
    int newIdx = m_currentInstructionIndex;
    for (int i = m_currentInstructionIndex + 1; i < m_route.instructions.size(); ++i) {
        if (m_route.instructions[i].originalShapeIndex <= m_routeWaypointIndex) {
            newIdx = i;
        } else {
            break;
        }
    }

    if (newIdx != m_currentInstructionIndex) {
        m_currentInstructionIndex = newIdx;
        const auto &instr = m_route.instructions[newIdx];
        if (!instr.streetName.isEmpty()) {
            setRoadName(instr.streetName);
        }
        // Derive a plausible speed limit from the maneuver type
        switch (instr.type) {
            case ManeuverType::RoundaboutEnter:
            case ManeuverType::RoundaboutExit:
            case ManeuverType::TurnSharpLeft:
            case ManeuverType::TurnSharpRight:
            case ManeuverType::UTurn:
            case ManeuverType::UTurnRight:
                setSpeedLimit(QStringLiteral("30"));
                setRoadType(QStringLiteral("residential"));
                break;
            case ManeuverType::TurnLeft:
            case ManeuverType::TurnRight:
            case ManeuverType::TurnSlightLeft:
            case ManeuverType::TurnSlightRight:
                setSpeedLimit(QStringLiteral("30"));
                setRoadType(QStringLiteral("secondary"));
                break;
            case ManeuverType::ExitLeft:
            case ManeuverType::ExitRight:
            case ManeuverType::MergeStraight:
            case ManeuverType::MergeLeft:
            case ManeuverType::MergeRight:
                setSpeedLimit(QStringLiteral("60"));
                setRoadType(QStringLiteral("primary"));
                break;
            case ManeuverType::KeepStraight:
            case ManeuverType::KeepLeft:
            case ManeuverType::KeepRight:
            default:
                setSpeedLimit(QStringLiteral("50"));
                setRoadType(QStringLiteral("secondary"));
                break;
        }
    }
}

void SimulatorService::applyDefaults()
{
    // Set initial settings
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.theme"), QStringLiteral("dark"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.language"), QStringLiteral("en"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.battery-display-mode"), QStringLiteral("percentage"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.map.type"), QStringLiteral("online"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("scooter.dual-battery"), QStringLiteral("true"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.show-gps"), QStringLiteral("always"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.show-bluetooth"), QStringLiteral("always"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.show-cloud"), QStringLiteral("always"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.show-internet"), QStringLiteral("always"));
    m_repo->set(QStringLiteral("settings"), QStringLiteral("dashboard.show-clock"), QStringLiteral("always"));

    // Engine firmware
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("fw-version"), QStringLiteral("2.1.0-sim"));
    m_repo->set(QStringLiteral("engine-ecu"), QStringLiteral("kers"), QStringLiteral("on"));

    // Battery metadata
    for (int slot = 0; slot < 2; ++slot) {
        const QString ch = QStringLiteral("battery:") + QString::number(slot);
        m_repo->set(ch, QStringLiteral("serial-number"),
                    QStringLiteral("SIM-BAT") + QString::number(slot) + QStringLiteral("-001"));
        m_repo->set(ch, QStringLiteral("fw-version"), QStringLiteral("1.3.0-sim"));
        m_repo->set(ch, QStringLiteral("manufacturing-date"), QStringLiteral("2024-01-15"));
        m_repo->set(ch, QStringLiteral("cycle-count"), QStringLiteral("150"));
        m_repo->set(ch, QStringLiteral("state-of-health"), QStringLiteral("95"));
        m_repo->set(ch, QStringLiteral("temperature-state"), QStringLiteral("normal"));
    }

    // BLE
    m_repo->set(QStringLiteral("ble"), QStringLiteral("mac-address"), QStringLiteral("AA:BB:CC:DD:EE:FF"));
    m_repo->set(QStringLiteral("ble"), QStringLiteral("pin-code"), QString());

    // Load the default parked preset after a short delay so stores have time to start
    QTimer::singleShot(200, this, [this]() {
        loadPreset(QStringLiteral("parked"));
    });
}

void SimulatorService::takeScreenshot()
{
    // Find the main ScootUI window (not the simulator panel)
    const auto windows = QGuiApplication::topLevelWindows();
    QQuickWindow *mainWindow = nullptr;
    for (auto *w : windows) {
        auto *qw = qobject_cast<QQuickWindow*>(w);
        if (qw && qw->title() == QLatin1String("ScootUI")) {
            mainWindow = qw;
            break;
        }
    }
    if (!mainWindow) {
        qWarning() << "Simulator: no main window found for screenshot";
        return;
    }

    QImage img = mainWindow->grabWindow();
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString path = dir + QStringLiteral("/scootui-") + timestamp + QStringLiteral(".png");
    if (img.save(path)) {
        qDebug() << "Simulator: screenshot saved to" << path;
    } else {
        qWarning() << "Simulator: failed to save screenshot to" << path;
    }
}

#include "VehicleStore.h"
#include "BlinkerCurve.h"
#include <QDateTime>
#include <QDebug>

VehicleStore::VehicleStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    if (m_repo) {
        m_repo->subscribe(QStringLiteral("buttons"), [this](const QString &ch, const QString &msg) {
            onButtonEvent(ch, msg);
        });
    }

    m_blinkTimer.setInterval(16); // ~60fps
    connect(&m_blinkTimer, &QTimer::timeout, this, &VehicleStore::updateBlinkClock);

    m_brakeLeftDebounce.setSingleShot(true);
    m_brakeLeftDebounce.setInterval(BRAKE_DEBOUNCE_MS);
    connect(&m_brakeLeftDebounce, &QTimer::timeout, this, [this]() {
        if (m_pendingBrakeLeft != m_brakeLeft) {
            m_brakeLeft = m_pendingBrakeLeft;
            emit brakeLeftChanged();
        }
    });

    m_brakeRightDebounce.setSingleShot(true);
    m_brakeRightDebounce.setInterval(BRAKE_DEBOUNCE_MS);
    connect(&m_brakeRightDebounce, &QTimer::timeout, this, [this]() {
        if (m_pendingBrakeRight != m_brakeRight) {
            m_brakeRight = m_pendingBrakeRight;
            emit brakeRightChanged();
        }
    });
}

VehicleStore::~VehicleStore()
{
    if (m_repo)
        m_repo->unsubscribe(QStringLiteral("buttons"));
}

SyncSettings VehicleStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("vehicle"),
        1000,
        {
            {QStringLiteral("blinkerState"), QStringLiteral("blinker:state")},
            {QStringLiteral("blinkerSwitch"), QStringLiteral("blinker:switch")},
            {QStringLiteral("brakeLeft"), QStringLiteral("brake:left")},
            {QStringLiteral("brakeRight"), QStringLiteral("brake:right")},
            {QStringLiteral("kickstand"), QStringLiteral("kickstand")},
            {QStringLiteral("state"), QStringLiteral("state")},
            {QStringLiteral("handleBarLockSensor"), QStringLiteral("handlebar:lock-sensor")},
            {QStringLiteral("handlebarInLockPosition"), QStringLiteral("handlebar:position")},
            {QStringLiteral("seatboxButton"), QStringLiteral("seatbox:button")},
            {QStringLiteral("seatboxLock"), QStringLiteral("seatbox:lock")},
            {QStringLiteral("hornButton"), QStringLiteral("horn-button")},
            {QStringLiteral("isUnableToDrive"), QStringLiteral("unable-to-drive")},
            {QStringLiteral("hopOnActive"), QStringLiteral("hop-on-active")},
            {QStringLiteral("mainPower"), QStringLiteral("main-power")},
        },
        {},
        {}
    };
}

void VehicleStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("blinker:state")) {
        auto v = ScootEnums::parseBlinkerState(value);
        if (v != m_blinkerState) {
            m_blinkerState = v;
            emit blinkerStateChanged();
            if (v != ScootEnums::BlinkerState::Off) {
                // Start the animation immediately at phase 0, then re-sync when
                // blinker:start_nanos arrives via HGET (typically a few ms on
                // local Redis). For the ~1-2 frames before resync the arrow is
                // at opacity ~0 anyway, so the correction isn't visible.
                m_blinkPhaseOffset = 0;
                m_blinkElapsed.start();
                m_blinkTimer.start();
                if (m_repo)
                    m_repo->requestField(QStringLiteral("vehicle"), QStringLiteral("blinker:start_nanos"));
            } else {
                m_blinkTimer.stop();
                if (m_blinkOpacity != 0.0) {
                    m_blinkOpacity = 0.0;
                    emit blinkOpacityChanged();
                }
            }
        }
    } else if (variable == QLatin1String("blinker:start_nanos")) {
        qint64 startNanos = value.toLongLong();
        if (startNanos > 0 && m_blinkerState != ScootEnums::BlinkerState::Off) {
            qint64 startMs = startNanos / 1000000LL;
            qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            qint64 phase = ((nowMs - startMs) % blinker::kCycleMs + blinker::kCycleMs) % blinker::kCycleMs;
            m_blinkPhaseOffset = phase;
            m_blinkElapsed.start();
        }
    } else if (variable == QLatin1String("blinker:switch")) {
        auto v = ScootEnums::parseBlinkerSwitch(value);
        if (v != m_blinkerSwitch) { m_blinkerSwitch = v; emit blinkerSwitchChanged(); }
    } else if (variable == QLatin1String("brake:left")) {
        setBrake(true, ScootEnums::parseToggle(value));
    } else if (variable == QLatin1String("brake:right")) {
        setBrake(false, ScootEnums::parseToggle(value));
    } else if (variable == QLatin1String("kickstand")) {
        auto v = ScootEnums::parseKickstand(value);
        if (v != m_kickstand) { m_kickstand = v; emit kickstandChanged(); }
    } else if (variable == QLatin1String("state")) {
        auto v = ScootEnums::parseVehicleState(value);
        if (v != m_state) { m_state = v; emit stateChanged(); }
        if (value != m_stateRaw) { m_stateRaw = value; emit stateRawChanged(); }
    } else if (variable == QLatin1String("handlebar:lock-sensor")) {
        auto v = ScootEnums::parseHandleBarLockSensor(value);
        if (v != m_handleBarLockSensor) { m_handleBarLockSensor = v; emit handleBarLockSensorChanged(); }
    } else if (variable == QLatin1String("handlebar:position")) {
        bool v = (value == QLatin1String("on-place"));
        if (v != m_handlebarInLockPosition) { m_handlebarInLockPosition = v; emit handlebarInLockPositionChanged(); }
    } else if (variable == QLatin1String("seatbox:button")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_seatboxButton) { m_seatboxButton = v; emit seatboxButtonChanged(); }
    } else if (variable == QLatin1String("seatbox:lock")) {
        auto v = ScootEnums::parseSeatboxLock(value);
        if (v != m_seatboxLock) { m_seatboxLock = v; emit seatboxLockChanged(); }
    } else if (variable == QLatin1String("horn-button")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_hornButton) { m_hornButton = v; emit hornButtonChanged(); }
    } else if (variable == QLatin1String("unable-to-drive")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_isUnableToDrive) { m_isUnableToDrive = v; emit isUnableToDriveChanged(); }
    } else if (variable == QLatin1String("hop-on-active")) {
        bool v = (value == QLatin1String("true"));
        if (v != m_hopOnActive) { m_hopOnActive = v; emit hopOnActiveChanged(); }
    } else if (variable == QLatin1String("main-power")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_mainPower) { m_mainPower = v; emit mainPowerChanged(); }
    }
}

void VehicleStore::onButtonEvent(const QString &, const QString &message)
{
    // Real vehicle-service (and the simulator) publish to the "buttons"
    // pub/sub channel in two formats:
    //   "brake:left:on" / "brake:left:off"     (3 parts)
    //   "brake:right:on" / "brake:right:off"   (3 parts)
    //   "horn:on" / "horn:off"                 (2 parts)
    //   "seatbox:on" / "seatbox:off"           (2 parts)
    //   "blinker:left:on" / "blinker:left:off" (3 parts)
    //   "blinker:right:on" / "blinker:right:off" (3 parts)
    QStringList parts = message.split(':');
    if (parts.isEmpty()) return;

    const QString &kind = parts[0];

    if (kind == QLatin1String("brake")) {
        if (parts.size() < 3) return;
        const QString &position = parts[1];
        auto v = ScootEnums::parseToggle(parts[2]);
        if (position == QLatin1String("left")) {
            setBrake(true, v);
        } else if (position == QLatin1String("right")) {
            setBrake(false, v);
        }
        return;
    }

    if (kind == QLatin1String("horn")) {
        if (parts.size() < 2) return;
        auto v = ScootEnums::parseToggle(parts[1]);
        if (v != m_hornButton) { m_hornButton = v; emit hornButtonChanged(); }
        return;
    }

    if (kind == QLatin1String("seatbox")) {
        if (parts.size() < 2) return;
        auto v = ScootEnums::parseToggle(parts[1]);
        if (v != m_seatboxButton) { m_seatboxButton = v; emit seatboxButtonChanged(); }
        return;
    }

    if (kind == QLatin1String("blinker")) {
        if (parts.size() < 3) return;
        const QString &position = parts[1];
        bool on = (parts[2] == QLatin1String("on"));
        // The dashboard mirrors the *physical* switch state. A press
        // on either side moves the switch into that position; a release
        // moves it back to Off. We never see both sides "on" at once
        // from the buttons channel — vehicle-service models that as
        // hazards via blinker:state, not the switch.
        ScootEnums::BlinkerSwitch desired = m_blinkerSwitch;
        if (on) {
            desired = (position == QLatin1String("left"))
                ? ScootEnums::BlinkerSwitch::Left
                : ScootEnums::BlinkerSwitch::Right;
        } else {
            // Only clear if the release matches the position we currently
            // think the switch is in — otherwise a stale "left:off" from
            // a previous toggle could clobber a fresh "right:on".
            if ((position == QLatin1String("left") && m_blinkerSwitch == ScootEnums::BlinkerSwitch::Left) ||
                (position == QLatin1String("right") && m_blinkerSwitch == ScootEnums::BlinkerSwitch::Right)) {
                desired = ScootEnums::BlinkerSwitch::Off;
            }
        }
        if (desired != m_blinkerSwitch) {
            m_blinkerSwitch = desired;
            emit blinkerSwitchChanged();
        }
        return;
    }
}

void VehicleStore::setBrake(bool isLeft, ScootEnums::Toggle value)
{
    if (isLeft) {
        m_pendingBrakeLeft = value;
        if (value == m_brakeLeft) {
            m_brakeLeftDebounce.stop();
        } else {
            m_brakeLeftDebounce.start();
        }
    } else {
        m_pendingBrakeRight = value;
        if (value == m_brakeRight) {
            m_brakeRightDebounce.stop();
        } else {
            m_brakeRightDebounce.start();
        }
    }
}

void VehicleStore::updateBlinkClock()
{
    // Sample the real hardware fade curve (fade10-blink) so the on-screen
    // arrow fades with exactly the shape and duration of the physical lamp.
    const qint64 phase = (m_blinkElapsed.elapsed() + m_blinkPhaseOffset) % blinker::kCycleMs;
    const qreal opacity = blinker::sample(static_cast<int>(phase));

    if (!qFuzzyCompare(1.0 + opacity, 1.0 + m_blinkOpacity)) {
        m_blinkOpacity = opacity;
        emit blinkOpacityChanged();
    }
}

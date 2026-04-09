#include "VehicleStore.h"
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
                // Request start_nanos to compute correct phase; animation starts in its handler.
                // Falls back to phase 0 if start_nanos arrives after this or is unavailable.
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
            m_blinkPhaseOffset = (nowMs - startMs) % 800;
            // Restart elapsed timer so phase is counted from now with the computed offset
            m_blinkElapsed.start();
        }
    } else if (variable == QLatin1String("blinker:switch")) {
        auto v = ScootEnums::parseBlinkerSwitch(value);
        if (v != m_blinkerSwitch) { m_blinkerSwitch = v; emit blinkerSwitchChanged(); }
    } else if (variable == QLatin1String("brake:left")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_brakeLeft) { m_brakeLeft = v; emit brakeLeftChanged(); }
    } else if (variable == QLatin1String("brake:right")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_brakeRight) { m_brakeRight = v; emit brakeRightChanged(); }
    } else if (variable == QLatin1String("kickstand")) {
        auto v = ScootEnums::parseKickstand(value);
        if (v != m_kickstand) { m_kickstand = v; emit kickstandChanged(); }
    } else if (variable == QLatin1String("state")) {
        auto v = ScootEnums::parseScooterState(value);
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
    }
}

void VehicleStore::onButtonEvent(const QString &, const QString &message)
{
    // Format: "brake:left/right:on/off"
    QStringList parts = message.split(':');
    if (parts.size() < 3 || parts[0] != QLatin1String("brake")) return;

    QString position = parts[1];
    QString state = parts[2];
    auto v = ScootEnums::parseToggle(state);

    if (position == QLatin1String("left")) {
        if (v != m_brakeLeft) {
            m_brakeLeft = v;
            emit brakeLeftChanged();
        }
    } else if (position == QLatin1String("right")) {
        if (v != m_brakeRight) {
            m_brakeRight = v;
            emit brakeRightChanged();
        }
    }
}

void VehicleStore::updateBlinkClock()
{
    constexpr int FADE_IN = 250;
    constexpr int FADE_OUT = 250;
    constexpr int PAUSE = 300;
    constexpr int CYCLE = FADE_IN + FADE_OUT + PAUSE; // 800ms, matches vehicle-service blinkerInterval

    const int phase = static_cast<int>((m_blinkElapsed.elapsed() + m_blinkPhaseOffset) % CYCLE);
    qreal opacity;

    if (phase < FADE_IN) {
        opacity = m_blinkEasing.valueForProgress(phase / qreal(FADE_IN));
    } else if (phase < FADE_IN + FADE_OUT) {
        opacity = 1.0 - m_blinkEasing.valueForProgress((phase - FADE_IN) / qreal(FADE_OUT));
    } else {
        opacity = 0.0;
    }

    if (!qFuzzyCompare(1.0 + opacity, 1.0 + m_blinkOpacity)) {
        m_blinkOpacity = opacity;
        emit blinkOpacityChanged();
    }
}

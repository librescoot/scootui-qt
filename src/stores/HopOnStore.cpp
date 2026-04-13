#include "HopOnStore.h"

#include "VehicleStore.h"
#include "SettingsStore.h"
#include "DashboardStore.h"
#include "services/SettingsService.h"
#include "repositories/MdbRepository.h"
#include "models/Enums.h"

#include <QDebug>

HopOnStore::HopOnStore(VehicleStore *vehicle,
                       SettingsStore *settings,
                       SettingsService *settingsService,
                       DashboardStore *dashboard,
                       MdbRepository *repo,
                       QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
    , m_settings(settings)
    , m_settingsService(settingsService)
    , m_dashboard(dashboard)
    , m_repo(repo)
{
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(kIdleTimeoutMs);
    connect(&m_idleTimer, &QTimer::timeout, this, &HopOnStore::onIdleTimeout);

    m_countdownTimer.setSingleShot(false);
    m_countdownTimer.setInterval(kCountdownTickMs);
    connect(&m_countdownTimer, &QTimer::timeout, this, &HopOnStore::onCountdownTick);

    m_backlightTimer.setSingleShot(true);
    m_backlightTimer.setInterval(kBacklightDelayMs);
    connect(&m_backlightTimer, &QTimer::timeout,
            this, &HopOnStore::onBacklightDelayElapsed);

    // Watch every input we care about for combo capture / matching.
    connect(m_vehicle, &VehicleStore::brakeLeftChanged,
            this, &HopOnStore::onBrakeLeftChanged);
    connect(m_vehicle, &VehicleStore::brakeRightChanged,
            this, &HopOnStore::onBrakeRightChanged);
    connect(m_vehicle, &VehicleStore::hornButtonChanged,
            this, &HopOnStore::onHornButtonChanged);
    connect(m_vehicle, &VehicleStore::blinkerSwitchChanged,
            this, &HopOnStore::onBlinkerSwitchChanged);
    connect(m_vehicle, &VehicleStore::seatboxButtonChanged,
            this, &HopOnStore::onSeatboxButtonChanged);

    // Auto-cleanup if the FSM leaves Parked while we're locked
    // (e.g. auto-standby timer fired in the background).
    connect(m_vehicle, &VehicleStore::stateChanged,
            this, &HopOnStore::onVehicleStateChanged);
    connect(m_vehicle, &VehicleStore::hopOnActiveChanged,
            this, &HopOnStore::onHopOnActiveChanged);

    // Settings update -> emit comboChanged so UI rebuilds the menu.
    connect(m_settings, &SettingsStore::hopOnComboChanged,
            this, &HopOnStore::comboChanged);

    // Initialise edge-detection state from current values so the very
    // first signal isn't treated as a press.
    m_lastBrakeLeft = m_vehicle->brakeLeft();
    m_lastBrakeRight = m_vehicle->brakeRight();
    m_lastHornButton = m_vehicle->hornButton();
    m_lastBlinkerSwitch = m_vehicle->blinkerSwitch();
    m_lastSeatboxButton = m_vehicle->seatboxButton();
}

QString HopOnStore::combo() const
{
    return m_settings ? m_settings->hopOnCombo() : QString();
}

void HopOnStore::startLearning()
{
    if (!m_vehicle->isParked()) {
        qDebug() << "HopOn: refuse startLearning, not parked";
        return;
    }
    qDebug() << "HopOn: startLearning";

    // Borrow vehicle-service's StateHopOn silently so input side-effects
    // (horn, blinker, brake LED, seatbox open, hibernation hold) are
    // suppressed while the user records their combo. "engage-silent"
    // skips the LED cue, opportunistic steering lock, and hop-on-active
    // flag publish — only the input suppression is borrowed.
    if (m_repo)
        m_repo->push(QStringLiteral("scooter:hop-on"), QStringLiteral("engage-silent"));

    m_buffer.clear();
    emit capturedTokensChanged();
    setLastResult(ResultNone);
    setMode(Learning);
    resetIdleCountdown();
}

void HopOnStore::activate()
{
    if (!m_vehicle->isParked()) {
        qDebug() << "HopOn: refuse activate, not parked";
        return;
    }
    if (combo().isEmpty()) {
        qDebug() << "HopOn: refuse activate, no combo defined";
        return;
    }
    qDebug() << "HopOn: activate";

    // Tell vehicle-service to engage the lockout (and steering lock if positioned).
    if (m_repo)
        m_repo->push(QStringLiteral("scooter:hop-on"), QStringLiteral("engage"));

    // Mirror the OTA screen pattern: keep the backlight ON briefly so the
    // user can see the lock screen, then disable it after kBacklightDelayMs.
    if (m_dashboard)
        m_dashboard->setBacklightEnabled(true);
    m_backlightTimer.start();

    m_buffer.clear();
    emit capturedTokensChanged();
    setMode(Locked);
    resetIdleCountdown();
}

void HopOnStore::disable()
{
    qDebug() << "HopOn: disable (clearing combo)";
    if (m_settingsService)
        m_settingsService->updateHopOnCombo(QString());
    // m_settings will pick up the cleared field via Redis sync; we also
    // emit comboChanged eagerly so the menu refreshes immediately.
    emit comboChanged();
}

void HopOnStore::unlock()
{
    qDebug() << "HopOn: unlock";
    if (m_repo)
        m_repo->push(QStringLiteral("scooter:hop-on"), QStringLiteral("release"));
    m_backlightTimer.stop();
    if (m_dashboard)
        m_dashboard->setBacklightEnabled(true);

    cancelTimers();
    m_buffer.clear();
    emit capturedTokensChanged();
    setLastResult(ResultUnlocked);
    setMode(Idle);
}

void HopOnStore::onBacklightDelayElapsed()
{
    if (m_mode != Locked) return;
    qDebug() << "HopOn: backlight delay elapsed, turning backlight off";
    if (m_dashboard)
        m_dashboard->setBacklightEnabled(false);
}

void HopOnStore::onBrakeLeftChanged()
{
    int v = m_vehicle->brakeLeft();
    int prev = m_lastBrakeLeft;
    m_lastBrakeLeft = v;
    if (m_mode == Idle) return;
    // Off (1) -> On (0) is a press, in ScootEnums::Toggle ordering
    if (prev != static_cast<int>(ScootEnums::Toggle::On) &&
        v == static_cast<int>(ScootEnums::Toggle::On)) {
        pushToken(QStringLiteral("LB"));
    }
}

void HopOnStore::onBrakeRightChanged()
{
    int v = m_vehicle->brakeRight();
    int prev = m_lastBrakeRight;
    m_lastBrakeRight = v;
    if (m_mode == Idle) return;
    if (prev != static_cast<int>(ScootEnums::Toggle::On) &&
        v == static_cast<int>(ScootEnums::Toggle::On)) {
        pushToken(QStringLiteral("RB"));
    }
}

void HopOnStore::onHornButtonChanged()
{
    int v = m_vehicle->hornButton();
    int prev = m_lastHornButton;
    m_lastHornButton = v;
    if (m_mode == Idle) return;
    if (prev != static_cast<int>(ScootEnums::Toggle::On) &&
        v == static_cast<int>(ScootEnums::Toggle::On)) {
        pushToken(QStringLiteral("HORN"));
    }
}

void HopOnStore::onBlinkerSwitchChanged()
{
    int v = m_vehicle->blinkerSwitch();
    int prev = m_lastBlinkerSwitch;
    m_lastBlinkerSwitch = v;
    if (m_mode == Idle) return;
    // Treat any transition INTO Left or Right as a press; transition back
    // to Off is not a token.
    if (v == prev) return;
    if (v == static_cast<int>(ScootEnums::BlinkerSwitch::Left)) {
        pushToken(QStringLiteral("BL"));
    } else if (v == static_cast<int>(ScootEnums::BlinkerSwitch::Right)) {
        pushToken(QStringLiteral("BR"));
    }
}

void HopOnStore::onSeatboxButtonChanged()
{
    int v = m_vehicle->seatboxButton();
    int prev = m_lastSeatboxButton;
    m_lastSeatboxButton = v;
    if (m_mode == Idle) return;
    if (prev != static_cast<int>(ScootEnums::Toggle::On) &&
        v == static_cast<int>(ScootEnums::Toggle::On)) {
        pushToken(QStringLiteral("SBOX"));
    }
}

void HopOnStore::pushToken(const QString &token)
{
    qDebug() << "HopOn: pushToken" << token << "mode=" << m_mode;
    m_buffer.append(token);
    emit capturedTokensChanged();
    resetIdleCountdown();

    if (m_mode == Locked) {
        // Match against stored combo as a complete sequence. We compare
        // the full buffer (no rolling window): a wrong leading press
        // means the user must wait 5 s for the buffer to clear and
        // start fresh. This is intentional — partial-match unlocking
        // would let an attacker brute-force suffixes.
        const QStringList expected = combo().split(QLatin1Char('|'),
            #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts
            #else
            QString::SkipEmptyParts
            #endif
        );
        if (m_buffer == expected) {
            unlock();
        }
    }
}

void HopOnStore::resetIdleCountdown()
{
    m_idleMillisRemaining = kIdleTimeoutMs;
    emit idleMillisRemainingChanged();
    m_idleTimer.start();
    if (!m_countdownTimer.isActive())
        m_countdownTimer.start();
}

void HopOnStore::cancelTimers()
{
    m_idleTimer.stop();
    m_countdownTimer.stop();
    if (m_idleMillisRemaining != 0) {
        m_idleMillisRemaining = 0;
        emit idleMillisRemainingChanged();
    }
}

void HopOnStore::onIdleTimeout()
{
    m_countdownTimer.stop();
    m_idleMillisRemaining = 0;
    emit idleMillisRemainingChanged();

    if (m_mode == Learning) {
        if (m_buffer.size() >= kMinComboLength) {
            const QString joined = m_buffer.join(QLatin1Char('|'));
            qDebug() << "HopOn: learning saved" << joined;
            if (m_settingsService)
                m_settingsService->updateHopOnCombo(joined);
            setLastResult(ResultSaved);
        } else {
            qDebug() << "HopOn: learning aborted (buffer too short:" << m_buffer.size() << ")";
            setLastResult(ResultAborted);
        }
        // Release the silent StateHopOn that startLearning() entered.
        if (m_repo)
            m_repo->push(QStringLiteral("scooter:hop-on"), QStringLiteral("release"));
        m_buffer.clear();
        emit capturedTokensChanged();
        setMode(Idle);
    } else if (m_mode == Locked) {
        // 5 s of input idle while locked: just clear the buffer so the
        // user can start a fresh attempt. Stay locked.
        if (!m_buffer.isEmpty()) {
            qDebug() << "HopOn: locked input timeout, clearing buffer";
            m_buffer.clear();
            emit capturedTokensChanged();
        }
    }
}

void HopOnStore::onCountdownTick()
{
    m_idleMillisRemaining -= kCountdownTickMs;
    if (m_idleMillisRemaining < 0) m_idleMillisRemaining = 0;
    emit idleMillisRemainingChanged();
}

void HopOnStore::onVehicleStateChanged()
{
    if (m_mode == Idle) return;
    if (!m_vehicle->isParked()) {
        // Left Parked while learning or locked. Drop everything. We
        // also push "release" so vehicle-service doesn't end up stuck
        // in StateHopOn — if it already left (e.g. via auto-standby
        // timeout), the release is a harmless no-op.
        qDebug() << "HopOn: vehicle left Parked, exiting mode" << m_mode;
        if (m_repo)
            m_repo->push(QStringLiteral("scooter:hop-on"), QStringLiteral("release"));
        if (m_mode == Locked) {
            m_backlightTimer.stop();
            if (m_dashboard)
                m_dashboard->setBacklightEnabled(true);
        }
        cancelTimers();
        m_buffer.clear();
        emit capturedTokensChanged();
        setMode(Idle);
    }
}

void HopOnStore::onHopOnActiveChanged()
{
    // If vehicle-service spontaneously cleared hop-on (e.g. it restarted),
    // make the dashboard mirror it instead of staying stuck on the lock screen.
    if (!m_vehicle->hopOnActive() && m_mode == Locked) {
        qDebug() << "HopOn: vehicle-service released hop-on externally";
        m_backlightTimer.stop();
        if (m_dashboard)
            m_dashboard->setBacklightEnabled(true);
        cancelTimers();
        m_buffer.clear();
        emit capturedTokensChanged();
        setMode(Idle);
    }
}

void HopOnStore::setMode(Mode m)
{
    if (m == m_mode) return;
    m_mode = m;
    emit modeChanged();
}

void HopOnStore::setLastResult(Result r)
{
    if (r == m_lastResult) return;
    m_lastResult = r;
    emit lastResultChanged();
}

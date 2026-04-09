#pragma once

#include <QObject>
#include <QStringList>
#include <QString>
#include <QTimer>

class VehicleStore;
class SettingsStore;
class SettingsService;
class DashboardStore;
class MdbRepository;

/**
 * HopOnStore — handles "hop-on / hop-off" mode for short stops.
 *
 * Modes:
 *   Idle     — nothing happening; nothing visible.
 *   Learning — capturing a new key combination from the user. Live tokens
 *              are exposed via capturedTokens for visualisation; the buffer
 *              is committed to settings after 5 s of input idle, provided
 *              at least 2 tokens were captured (otherwise it aborts).
 *   Locked   — hop-on engaged. Display backlight is off, the FSM in
 *              vehicle-service blocks Parked->ReadyToDrive, and we watch
 *              the same input transitions for the configured combo.
 *
 * Combo storage: pipe-delimited token list under settings field
 * `dashboard.hop-on-combo`. Tokens:
 *   LB   left brake press
 *   RB   right brake press
 *   HORN horn button press
 *   BL   blinker switch -> left
 *   BR   blinker switch -> right
 *
 * Activation is from the menu only (see MenuStore). Unlock is by pressing
 * the stored combo while Locked.
 */
class HopOnStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QStringList capturedTokens READ capturedTokens NOTIFY capturedTokensChanged)
    Q_PROPERTY(int idleMillisRemaining READ idleMillisRemaining NOTIFY idleMillisRemainingChanged)
    Q_PROPERTY(QString combo READ combo NOTIFY comboChanged)
    Q_PROPERTY(bool hasCombo READ hasCombo NOTIFY comboChanged)
    Q_PROPERTY(int lastResult READ lastResult NOTIFY lastResultChanged)

public:
    enum Mode {
        Idle = 0,
        Learning = 1,
        Locked = 2,
    };
    Q_ENUM(Mode)

    enum Result {
        ResultNone = 0,
        ResultSaved = 1,
        ResultAborted = 2,
        ResultUnlocked = 3,
    };
    Q_ENUM(Result)

    explicit HopOnStore(VehicleStore *vehicle,
                        SettingsStore *settings,
                        SettingsService *settingsService,
                        DashboardStore *dashboard,
                        MdbRepository *repo,
                        QObject *parent = nullptr);

    int mode() const { return static_cast<int>(m_mode); }
    QStringList capturedTokens() const { return m_buffer; }
    int idleMillisRemaining() const { return m_idleMillisRemaining; }
    QString combo() const;
    bool hasCombo() const { return !combo().isEmpty(); }
    int lastResult() const { return static_cast<int>(m_lastResult); }

    Q_INVOKABLE void startLearning();
    Q_INVOKABLE void activate();
    Q_INVOKABLE void disable();

signals:
    void modeChanged();
    void capturedTokensChanged();
    void idleMillisRemainingChanged();
    void comboChanged();
    void lastResultChanged();

private slots:
    void onBrakeLeftChanged();
    void onBrakeRightChanged();
    void onHornButtonChanged();
    void onBlinkerSwitchChanged();
    void onSeatboxButtonChanged();
    void onVehicleStateChanged();
    void onHopOnActiveChanged();
    void onIdleTimeout();
    void onCountdownTick();
    void onBacklightDelayElapsed();

private:
    static constexpr int kIdleTimeoutMs = 5000;
    static constexpr int kCountdownTickMs = 100;
    static constexpr int kMinComboLength = 2;
    // Delay after entering hop-on Locked mode before the dashboard
    // backlight is turned off. Mirrors the OTA screen pattern at
    // qml/screens/OtaBackgroundScreen.qml. The user gets enough time to
    // see the lock screen and orient themselves before the display goes
    // dark.
    static constexpr int kBacklightDelayMs = 30000;

    void pushToken(const QString &token);
    void resetIdleCountdown();
    void cancelTimers();
    void unlock();
    void setMode(Mode m);
    void setLastResult(Result r);

    VehicleStore *m_vehicle;
    SettingsStore *m_settings;
    SettingsService *m_settingsService;
    DashboardStore *m_dashboard;
    MdbRepository *m_repo;

    Mode m_mode = Idle;
    QStringList m_buffer;
    int m_idleMillisRemaining = 0;
    Result m_lastResult = ResultNone;

    QTimer m_idleTimer;
    QTimer m_countdownTimer;
    QTimer m_backlightTimer;

    // Edge-detection state for input transitions
    int m_lastBrakeLeft = -1;
    int m_lastBrakeRight = -1;
    int m_lastHornButton = -1;
    int m_lastBlinkerSwitch = -1;
    int m_lastSeatboxButton = -1;
};

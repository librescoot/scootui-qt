#pragma once

#include <QObject>
#include <QTimer>

class VehicleStore;

/**
 * Centralized brake input handler.
 *
 * Watches VehicleStore brake signals and emits high-level gesture signals.
 * Pure gesture detector — no knowledge of screens or menu state.
 * Consumers connect to signals and apply their own context guards.
 */
class InputHandler : public QObject
{
    Q_OBJECT

public:
    explicit InputHandler(VehicleStore *vehicle, QObject *parent = nullptr);

signals:
    void leftTap();         // Left brake quick press-release (< hold threshold)
    void leftHold();        // Left brake held >= hold threshold then released
    void leftDoubleTap();   // Two left taps within double-tap window
    void rightTap();        // Right brake press-release (any duration)

private slots:
    void onBrakeLeftChanged();
    void onBrakeRightChanged();

private:
    void handlePress(bool isLeft);
    void handleRelease(bool isLeft);

    VehicleStore *m_vehicle;

    static constexpr int DOUBLE_TAP_DELAY_MS = 300;
    static constexpr int HOLD_THRESHOLD_MS = 500;

    struct {
        bool wasOn = false;
        bool holdFired = false;
        bool waitingSecondTap = false;
        QTimer *doubleTapTimer = nullptr;
        QTimer *holdTimer = nullptr;
    } m_left;

    struct {
        bool wasOn = false;
    } m_right;
};

#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class VehicleStore;
class MenuStore;

class QQmlEngine;
class QJSEngine;

class InputHandler : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT

public:
    explicit InputHandler(VehicleStore *vehicle, MenuStore *menu, QObject *parent = nullptr);
    static InputHandler *create(QQmlEngine *, QJSEngine *) { return s_instance; }

signals:
    // Gesture signals (only emitted when menu is closed and vehicle is parked)
    void leftTap();       // Left brake quick press-release (< hold threshold)
    void leftHold();      // Left brake held >= hold threshold then released
    void rightTap();      // Right brake press-release (any duration)

private slots:
    void onBrakeLeftChanged();
    void onBrakeRightChanged();

private:
    void handlePress(bool isLeft);
    void handleRelease(bool isLeft);

    VehicleStore *m_vehicle;
    MenuStore *m_menu;

    static constexpr int DOUBLE_TAP_DELAY_MS = 300;
    static constexpr int HOLD_THRESHOLD_MS = 500;

    // Left brake state
    struct {
        bool wasOn = false;
        bool gestureActive = false;   // Press started while menu was closed
        bool holdFired = false;       // Hold timer expired before release
        bool waitingSecondTap = false;
        QTimer *doubleTapTimer = nullptr;
        QTimer *holdTimer = nullptr;
    } m_left;

    // Right brake state
    struct {
        bool wasOn = false;
        bool gestureActive = false;   // Press started while menu was closed
    } m_right;
    static inline InputHandler *s_instance = nullptr;
};

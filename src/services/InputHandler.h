#pragma once

#include <QObject>
#include <QTimer>

class VehicleStore;
class MenuStore;

class InputHandler : public QObject
{
    Q_OBJECT

public:
    explicit InputHandler(VehicleStore *vehicle, MenuStore *menu, QObject *parent = nullptr);

private slots:
    void onBrakeLeftChanged();
    void onBrakeRightChanged();

private:
    void handlePress(bool isLeft);
    void handleRelease(bool isLeft);

    VehicleStore *m_vehicle;
    MenuStore *m_menu;

    // Gesture detection state per side
    struct SideState {
        QTimer *doubleTapTimer = nullptr;
        bool waitingSecondTap = false;
        bool wasOn = false;
    };

    SideState m_left;
    SideState m_right;

    static constexpr int DOUBLE_TAP_DELAY_MS = 300;
};

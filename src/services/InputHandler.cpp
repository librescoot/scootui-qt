#include "InputHandler.h"
#include "stores/VehicleStore.h"
#include "stores/MenuStore.h"
#include "models/Enums.h"

InputHandler::InputHandler(VehicleStore *vehicle, MenuStore *menu, QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
    , m_menu(menu)
{
    // Double-tap timer for left brake (menu open gesture)
    m_left.doubleTapTimer = new QTimer(this);
    m_left.doubleTapTimer->setSingleShot(true);
    m_left.doubleTapTimer->setInterval(DOUBLE_TAP_DELAY_MS);
    connect(m_left.doubleTapTimer, &QTimer::timeout, this, [this]() {
        m_left.waitingSecondTap = false;
    });

    // Hold timer for left brake (tap vs hold distinction)
    m_left.holdTimer = new QTimer(this);
    m_left.holdTimer->setSingleShot(true);
    m_left.holdTimer->setInterval(HOLD_THRESHOLD_MS);
    connect(m_left.holdTimer, &QTimer::timeout, this, [this]() {
        m_left.holdFired = true;
    });

    // Initialize previous state from current values
    m_left.wasOn = (vehicle->brakeLeft() == static_cast<int>(ScootEnums::Toggle::On));
    m_right.wasOn = (vehicle->brakeRight() == static_cast<int>(ScootEnums::Toggle::On));

    connect(m_vehicle, &VehicleStore::brakeLeftChanged,
            this, &InputHandler::onBrakeLeftChanged);
    connect(m_vehicle, &VehicleStore::brakeRightChanged,
            this, &InputHandler::onBrakeRightChanged);
}

void InputHandler::onBrakeLeftChanged()
{
    bool isOn = (m_vehicle->brakeLeft() == static_cast<int>(ScootEnums::Toggle::On));
    bool wasOn = m_left.wasOn;
    m_left.wasOn = isOn;

    if (!wasOn && isOn) {
        handlePress(true);
    } else if (wasOn && !isOn) {
        handleRelease(true);
    }
}

void InputHandler::onBrakeRightChanged()
{
    bool isOn = (m_vehicle->brakeRight() == static_cast<int>(ScootEnums::Toggle::On));
    bool wasOn = m_right.wasOn;
    m_right.wasOn = isOn;

    if (!wasOn && isOn) {
        handlePress(false);
    } else if (wasOn && !isOn) {
        handleRelease(false);
    }
}

void InputHandler::handlePress(bool isLeft)
{
    if (!m_vehicle->isParked()) return;

    if (m_menu->isOpen()) {
        // Menu consumes the press — don't track for gestures
        if (isLeft) {
            m_menu->navigateDown();
        } else {
            m_menu->selectItem();
        }
        return;
    }

    // Menu is closed — start gesture tracking
    if (isLeft) {
        m_left.gestureActive = true;
        m_left.holdFired = false;
        m_left.holdTimer->start();
    } else {
        m_right.gestureActive = true;
    }
}

void InputHandler::handleRelease(bool isLeft)
{
    if (!m_vehicle->isParked()) return;

    if (isLeft) {
        m_left.holdTimer->stop();

        if (m_left.gestureActive) {
            m_left.gestureActive = false;

            if (m_left.holdFired) {
                emit leftHold();
            } else {
                emit leftTap();

                // Double-tap detection for opening menu
                if (!m_menu->isOpen()) {
                    if (m_left.waitingSecondTap) {
                        m_left.doubleTapTimer->stop();
                        m_left.waitingSecondTap = false;
                        m_menu->open();
                    } else {
                        m_left.waitingSecondTap = true;
                        m_left.doubleTapTimer->start();
                    }
                }
            }
        }
        // else: press was consumed by menu — ignore release
    } else {
        if (m_right.gestureActive) {
            m_right.gestureActive = false;
            emit rightTap();
        }
        // else: press was consumed by menu — ignore release
    }
}

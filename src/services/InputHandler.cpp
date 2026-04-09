#include "InputHandler.h"
#include "stores/VehicleStore.h"
#include "models/Enums.h"

InputHandler::InputHandler(VehicleStore *vehicle, QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
{
    m_left.doubleTapTimer = new QTimer(this);
    m_left.doubleTapTimer->setSingleShot(true);
    m_left.doubleTapTimer->setInterval(DOUBLE_TAP_DELAY_MS);
    connect(m_left.doubleTapTimer, &QTimer::timeout, this, [this]() {
        m_left.waitingSecondTap = false;
    });

    m_left.holdTimer = new QTimer(this);
    m_left.holdTimer->setSingleShot(true);
    m_left.holdTimer->setInterval(HOLD_THRESHOLD_MS);
    connect(m_left.holdTimer, &QTimer::timeout, this, [this]() {
        m_left.holdFired = true;
    });

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

    if (isLeft) {
        m_left.holdFired = false;
        m_left.holdTimer->start();
    }
}

void InputHandler::handleRelease(bool isLeft)
{
    if (!m_vehicle->isParked()) return;

    if (isLeft) {
        m_left.holdTimer->stop();

        if (m_left.holdFired) {
            emit leftHold();
        } else {
            emit leftTap();

            if (m_left.waitingSecondTap) {
                m_left.doubleTapTimer->stop();
                m_left.waitingSecondTap = false;
                emit leftDoubleTap();
            } else {
                m_left.waitingSecondTap = true;
                m_left.doubleTapTimer->start();
            }
        }
    } else {
        emit rightTap();
    }
}

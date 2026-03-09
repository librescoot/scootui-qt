#include "InputHandler.h"
#include "stores/VehicleStore.h"
#include "stores/MenuStore.h"
#include "models/Enums.h"

InputHandler::InputHandler(VehicleStore *vehicle, MenuStore *menu, QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
    , m_menu(menu)
{
    // Double-tap timer for left brake (menu open when closed)
    m_left.doubleTapTimer = new QTimer(this);
    m_left.doubleTapTimer->setSingleShot(true);
    m_left.doubleTapTimer->setInterval(DOUBLE_TAP_DELAY_MS);
    connect(m_left.doubleTapTimer, &QTimer::timeout, this, [this]() {
        m_left.waitingSecondTap = false;
    });

    // Double-tap timer for right brake (unused, but initialized)
    m_right.doubleTapTimer = new QTimer(this);
    m_right.doubleTapTimer->setSingleShot(true);
    m_right.doubleTapTimer->setInterval(DOUBLE_TAP_DELAY_MS);
    connect(m_right.doubleTapTimer, &QTimer::timeout, this, [this]() {
        m_right.waitingSecondTap = false;
    });

    // Initialize previous state
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
    // Only handle when parked
    if (!m_vehicle->isParked()) return;

    // When menu is open, act on press (like Flutter)
    if (m_menu->isOpen()) {
        if (isLeft) {
            m_menu->navigateDown();
        } else {
            m_menu->selectItem();
        }
    }
}

void InputHandler::handleRelease(bool isLeft)
{
    if (!m_vehicle->isParked()) return;

    auto &side = isLeft ? m_left : m_right;

    // Double-tap detection (only for left brake when menu is closed)
    if (isLeft && !m_menu->isOpen()) {
        if (side.waitingSecondTap) {
            side.doubleTapTimer->stop();
            side.waitingSecondTap = false;
            // Double-tap detected → open menu
            m_menu->open();
        } else {
            side.waitingSecondTap = true;
            side.doubleTapTimer->start();
        }
    }
}

#include "ShortcutMenuController.h"
#include "stores/ThemeStore.h"
#include "stores/VehicleStore.h"
#include "core/Navigator.h"
#include "stores/DashboardStore.h"
#include "services/InputHandler.h"
#include "services/SettingsService.h"
#include "commands/CommandBus.h"
#include "models/Enums.h"
#include <QDebug>

ShortcutMenuController::ShortcutMenuController(ThemeStore *theme, VehicleStore *vehicle,
                                     Navigator *screen, DashboardStore *dashboard,
                                     InputHandler *input, CommandBus *commands,
                                     SettingsService *settingsService,
                                     QObject *parent)
    : QObject(parent)
    , m_theme(theme)
    , m_vehicle(vehicle)
    , m_navigator(screen)
    , m_dashboardStore(dashboard)
    , m_commands(commands)
    , m_settingsService(settingsService)
    , m_confirmTimer(new QTimer(this))
    , m_cycleTimer(new QTimer(this))
{
    m_confirmTimer->setSingleShot(true);
    m_confirmTimer->setInterval(CONFIRM_TIMEOUT_MS);
    connect(m_confirmTimer, &QTimer::timeout, this, &ShortcutMenuController::resetState);

    m_cycleTimer->setInterval(ITEM_CYCLE_MS);
    connect(m_cycleTimer, &QTimer::timeout, this, &ShortcutMenuController::onCycleTimeout);

    if (input) {
        connect(input, &InputHandler::seatboxLongTap, this, &ShortcutMenuController::onSeatboxLongTap);
        connect(input, &InputHandler::seatboxRelease, this, &ShortcutMenuController::onSeatboxRelease);
        connect(input, &InputHandler::seatboxPress, this, &ShortcutMenuController::onSeatboxPress);
        connect(input, &InputHandler::seatboxDoubleTap, this, &ShortcutMenuController::onSeatboxDoubleTap);
    }
}

void ShortcutMenuController::show()
{
    if (!m_visible) {
        m_visible = true;
        m_selectedIndex = 0;
        m_confirming = false;
        emit visibleChanged();
        emit selectionChanged();
        emit confirmingChanged();
    }
}

void ShortcutMenuController::hide()
{
    if (m_visible) {
        m_visible = false;
        m_confirming = false;
        m_confirmTimer->stop();
        m_cycleTimer->stop();
        emit visibleChanged();
        emit confirmingChanged();
    }
}

void ShortcutMenuController::cycle()
{
    m_selectedIndex = (m_selectedIndex + 1) % ITEM_COUNT;
    emit selectionChanged();
}

void ShortcutMenuController::confirm()
{
    if (!m_visible) return;

    m_confirming = true;
    emit confirmingChanged();
    m_confirmTimer->start();
}

bool ShortcutMenuController::isReadyToDrive() const
{
    return m_vehicle->state() == static_cast<int>(ScootEnums::VehicleState::ReadyToDrive);
}

void ShortcutMenuController::onSeatboxLongTap()
{
    if (!isReadyToDrive())
        return;
    // Open the menu and begin cycling items while the user keeps holding.
    if (!m_visible) {
        show();
        m_cycleTimer->start();
    }
}

void ShortcutMenuController::onSeatboxRelease()
{
    if (!isReadyToDrive())
        return;
    // Release after the menu is shown enters the confirmation window.
    if (m_visible && !m_confirming) {
        m_cycleTimer->stop();
        m_confirming = true;
        emit confirmingChanged();
        m_confirmTimer->start();
    }
}

void ShortcutMenuController::onSeatboxPress()
{
    if (!isReadyToDrive())
        return;
    // A press while confirming executes the selected action.
    if (m_confirming) {
        executeAction(m_selectedIndex);
        resetState();
    }
}

void ShortcutMenuController::onSeatboxDoubleTap()
{
    if (!isReadyToDrive())
        return;
    // Double-tap with the menu closed is a hazards toggle shortcut.
    if (!m_visible && !m_confirming)
        toggleHazards();
}

void ShortcutMenuController::onCycleTimeout()
{
    cycle();
}

void ShortcutMenuController::executeAction(int index)
{
    switch (index) {
    case 0: cycleTheme(); break;
    case 1: toggleView(); break;
    case 2: toggleHazards(); break;
    case 3: toggleDebugOverlay(); break;
    }
}

void ShortcutMenuController::toggleDebugOverlay()
{
    if (!m_commands || !m_dashboardStore) return;

    bool isOverlay = (m_dashboardStore->debugMode() == QLatin1String("overlay"));
    m_commands->setDebugMode(isOverlay ? QStringLiteral("off") : QStringLiteral("overlay"));
}

void ShortcutMenuController::toggleHazards()
{
    if (!m_commands) return;
    m_commands->toggleHazards(m_vehicle->blinkerState());
}

void ShortcutMenuController::toggleView()
{
    if (!m_navigator) return;

    const ScootEnums::ScreenMode current = m_navigator->currentScreenMode();
    if (current == ScootEnums::ScreenMode::Cluster) {
        m_navigator->setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
        m_settingsService->updateMode(QStringLiteral("navigation"));
    } else if (current == ScootEnums::ScreenMode::Map) {
        m_navigator->setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
        m_settingsService->updateMode(QStringLiteral("speedometer"));
    }
}

void ShortcutMenuController::cycleTheme()
{
    if (m_theme->isAutoMode()) {
        m_settingsService->updateTheme(QStringLiteral("dark"));
    } else if (m_theme->isDark()) {
        m_settingsService->updateTheme(QStringLiteral("light"));
    } else {
        m_settingsService->updateTheme(QStringLiteral("auto"));
    }
}

void ShortcutMenuController::resetState()
{
    m_cycleTimer->stop();
    m_confirmTimer->stop();

    m_visible = false;
    m_confirming = false;
    m_selectedIndex = 0;

    emit visibleChanged();
    emit confirmingChanged();
    emit selectionChanged();
}

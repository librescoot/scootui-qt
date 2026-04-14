#include "ShortcutMenuStore.h"
#include "ThemeStore.h"
#include "VehicleStore.h"
#include "ScreenStore.h"
#include "DashboardStore.h"
#include "../repositories/MdbRepository.h"
#include "../services/SettingsService.h"
#include "../models/Enums.h"
#include <QDebug>

namespace {
constexpr char kInputEventsChannel[] = "input-events";
}

ShortcutMenuStore::ShortcutMenuStore(ThemeStore *theme, VehicleStore *vehicle,
                                     ScreenStore *screen, DashboardStore *dashboard,
                                     MdbRepository *repo, SettingsService *settingsService,
                                     QObject *parent)
    : QObject(parent)
    , m_theme(theme)
    , m_vehicle(vehicle)
    , m_screenStore(screen)
    , m_dashboardStore(dashboard)
    , m_repo(repo)
    , m_settingsService(settingsService)
    , m_confirmTimer(new QTimer(this))
    , m_cycleTimer(new QTimer(this))
{
    m_confirmTimer->setSingleShot(true);
    m_confirmTimer->setInterval(CONFIRM_TIMEOUT_MS);
    connect(m_confirmTimer, &QTimer::timeout, this, &ShortcutMenuStore::resetState);

    m_cycleTimer->setInterval(ITEM_CYCLE_MS);
    connect(m_cycleTimer, &QTimer::timeout, this, &ShortcutMenuStore::onCycleTimeout);

    if (m_repo) {
        m_repo->subscribe(QLatin1String(kInputEventsChannel),
                          [this](const QString &, const QString &message) {
            onInputEvent(message);
        });
    }
}

ShortcutMenuStore::~ShortcutMenuStore()
{
    if (m_repo)
        m_repo->unsubscribe(QLatin1String(kInputEventsChannel));
}

void ShortcutMenuStore::show()
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

void ShortcutMenuStore::hide()
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

void ShortcutMenuStore::cycle()
{
    m_selectedIndex = (m_selectedIndex + 1) % ITEM_COUNT;
    emit selectionChanged();
}

void ShortcutMenuStore::confirm()
{
    if (!m_visible) return;

    m_confirming = true;
    emit confirmingChanged();
    m_confirmTimer->start();
}

bool ShortcutMenuStore::isReadyToDrive() const
{
    return m_vehicle->state() == static_cast<int>(ScootEnums::VehicleState::ReadyToDrive);
}

void ShortcutMenuStore::onInputEvent(const QString &message)
{
    // Format: "<source>:<gesture>" — only seatbox is of interest here.
    QStringList parts = message.split(':');
    if (parts.size() != 2 || parts[0] != QLatin1String("seatbox"))
        return;

    if (!isReadyToDrive())
        return;

    const QString &gesture = parts[1];

    if (gesture == QLatin1String("long-tap")) {
        // Open the menu and begin cycling items while the user keeps holding.
        if (!m_visible) {
            show();
            m_cycleTimer->start();
        }
    } else if (gesture == QLatin1String("release")) {
        // Release after the menu is shown enters the confirmation window.
        if (m_visible && !m_confirming) {
            m_cycleTimer->stop();
            m_confirming = true;
            emit confirmingChanged();
            m_confirmTimer->start();
        }
    } else if (gesture == QLatin1String("press")) {
        // A press while confirming executes the selected action.
        if (m_confirming) {
            executeAction(m_selectedIndex);
            resetState();
        }
    } else if (gesture == QLatin1String("double-tap")) {
        // Double-tap with the menu closed is a hazards toggle shortcut.
        if (!m_visible && !m_confirming) {
            toggleHazards();
        }
    }
}

void ShortcutMenuStore::onCycleTimeout()
{
    cycle();
}

void ShortcutMenuStore::executeAction(int index)
{
    switch (index) {
    case 0: cycleTheme(); break;
    case 1: toggleView(); break;
    case 2: toggleHazards(); break;
    case 3: toggleDebugOverlay(); break;
    }
}

void ShortcutMenuStore::toggleDebugOverlay()
{
    if (!m_repo || !m_dashboardStore) return;

    bool isOverlay = (m_dashboardStore->debugMode() == QLatin1String("overlay"));
    m_repo->set(QStringLiteral("dashboard"), QStringLiteral("debug"),
                isOverlay ? QStringLiteral("off") : QStringLiteral("overlay"));
}

void ShortcutMenuStore::toggleHazards()
{
    if (!m_repo) return;

    bool isBoth = (m_vehicle->blinkerState() == static_cast<int>(ScootEnums::BlinkerState::Both));

    m_repo->push(QStringLiteral("scooter:blinker"),
                 isBoth ? QStringLiteral("off") : QStringLiteral("both"));
}

void ShortcutMenuStore::toggleView()
{
    if (!m_screenStore) return;

    // Toggle between Cluster (0) and Map (1)
    int current = m_screenStore->currentScreen();
    if (current == 0) {
        m_screenStore->setScreen(1);
        m_settingsService->updateMode(QStringLiteral("navigation"));
    } else if (current == 1) {
        m_screenStore->setScreen(0);
        m_settingsService->updateMode(QStringLiteral("speedometer"));
    }
}

void ShortcutMenuStore::cycleTheme()
{
    if (m_theme->isDark()) {
        m_settingsService->updateTheme(QStringLiteral("light"));
    } else {
        m_settingsService->updateTheme(QStringLiteral("dark"));
    }
}

void ShortcutMenuStore::resetState()
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

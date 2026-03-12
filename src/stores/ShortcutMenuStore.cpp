#include "ShortcutMenuStore.h"
#include "ThemeStore.h"
#include "VehicleStore.h"
#include "ScreenStore.h"
#include "../repositories/MdbRepository.h"
#include "../services/SettingsService.h"
#include "../models/Enums.h"
#include <QDebug>

ShortcutMenuStore::ShortcutMenuStore(ThemeStore *theme, VehicleStore *vehicle,
                                     ScreenStore *screen, MdbRepository *repo,
                                     SettingsService *settingsService,
                                     QObject *parent)
    : QObject(parent)
    , m_theme(theme)
    , m_vehicle(vehicle)
    , m_screenStore(screen)
    , m_repo(repo)
    , m_settingsService(settingsService)
    , m_confirmTimer(new QTimer(this))
    , m_longPressTimer(new QTimer(this))
    , m_cycleTimer(new QTimer(this))
{
    m_confirmTimer->setSingleShot(true);
    m_confirmTimer->setInterval(CONFIRM_TIMEOUT_MS);
    connect(m_confirmTimer, &QTimer::timeout, this, &ShortcutMenuStore::resetState);

    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(LONG_PRESS_MS);
    connect(m_longPressTimer, &QTimer::timeout, this, &ShortcutMenuStore::onLongPressTimeout);

    m_cycleTimer->setInterval(ITEM_CYCLE_MS);
    connect(m_cycleTimer, &QTimer::timeout, this, &ShortcutMenuStore::onCycleTimeout);

    // Subscribe to button events
    if (m_repo) {
        m_repo->subscribe(QStringLiteral("buttons"), [this](const QString &channel, const QString &message) {
            handleButtonEvent(channel, message);
        });
    }
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

void ShortcutMenuStore::handleButtonEvent(const QString &channel, const QString &message)
{
    Q_UNUSED(channel);
    
    // Format: "button:state" (e.g., "seatbox:on")
    QStringList parts = message.split(':');
    if (parts.size() < 2) return;

    QString button = parts[0];
    QString state = parts[1];

    if (button != QLatin1String("seatbox")) return;

    // Only allow when ready-to-drive
    if (m_vehicle->state() != static_cast<int>(ScootEnums::ScooterState::ReadyToDrive)) {
        return;
    }

    if (state == QLatin1String("on")) {
        QDateTime now = QDateTime::currentDateTime();

        // 1. If confirming, execute selected action
        if (m_confirming) {
            executeAction(m_selectedIndex);
            resetState();
            return;
        }

        // 2. Check for double tap (toggle hazards)
        if (!m_buttonPressStartTime.isValid() && m_lastTapTime.isValid()) {
            if (m_lastTapTime.msecsTo(now) < DOUBLE_PRESS_MS) {
                toggleHazards();
                resetState();
                return;
            }
        }

        // 3. Start long press detection
        m_buttonPressStartTime = now;
        m_longPressTimer->start();

    } else if (state == QLatin1String("off")) {
        if (!m_buttonPressStartTime.isValid()) return;

        m_longPressTimer->stop();
        QDateTime now = QDateTime::currentDateTime();
        qint64 holdDuration = m_buttonPressStartTime.msecsTo(now);

        if (m_visible && !m_confirming) {
            // Enter confirmation state
            m_cycleTimer->stop();
            m_confirming = true;
            emit confirmingChanged();
            m_confirmTimer->start();
        } else if (holdDuration < LONG_PRESS_MS) {
            // Record tap time for potential double tap
            m_lastTapTime = m_buttonPressStartTime;
        }

        m_buttonPressStartTime = QDateTime();
    }
}

void ShortcutMenuStore::onLongPressTimeout()
{
    // Long press detected, show menu and start cycling
    show();
    m_cycleTimer->start();
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
    }
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
    } else if (current == 1) {
        m_screenStore->setScreen(0);
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
    m_longPressTimer->stop();
    m_cycleTimer->stop();
    m_confirmTimer->stop();
    
    m_visible = false;
    m_confirming = false;
    m_selectedIndex = 0;
    
    emit visibleChanged();
    emit confirmingChanged();
    emit selectionChanged();
}

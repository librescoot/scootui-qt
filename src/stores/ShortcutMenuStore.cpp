#include "ShortcutMenuStore.h"
#include "EngineStore.h"
#include "VehicleStore.h"
#include "ScreenStore.h"
#include "SavedLocationsStore.h"
#include "SettingsStore.h"
#include "../repositories/MdbRepository.h"
#include "../services/NavigationAvailabilityService.h"
#include "../services/SettingsService.h"
#include "../models/Enums.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr char kInputEventsChannel[] = "input-events";
}

ShortcutMenuStore::ShortcutMenuStore(EngineStore *engine, VehicleStore *vehicle,
                                     ScreenStore *screen, SavedLocationsStore *savedLocations,
                                     NavigationAvailabilityService *navigationAvailability,
                                     QObject *navigation, QObject *mapService,
                                     SettingsStore *settings, MdbRepository *repo,
                                     SettingsService *settingsService,
                                     QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_vehicle(vehicle)
    , m_screenStore(screen)
    , m_savedLocations(savedLocations)
    , m_navigationAvailability(navigationAvailability)
    , m_navigation(navigation)
    , m_mapService(mapService)
    , m_settings(settings)
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

    connect(m_vehicle, &VehicleStore::kickstandChanged, this, [this]() {
        if (m_visible
            && m_vehicle->kickstand() == static_cast<int>(ScootEnums::Kickstand::Up)) {
            resetState();
        }
    });
    connect(m_vehicle, &VehicleStore::stateChanged, this, [this]() {
        if (m_visible && !isReadyToDrive())
            resetState();
    });
    if (m_engine)
        connect(m_engine, &EngineStore::speedChanged, this, &ShortcutMenuStore::rebuildActions);
    if (m_savedLocations)
        connect(m_savedLocations, SIGNAL(locationsChanged()), this, SLOT(rebuildActions()));
    if (m_navigationAvailability)
        connect(m_navigationAvailability, SIGNAL(availabilityChanged()),
                this, SLOT(rebuildActions()));
    if (m_navigation)
        connect(m_navigation, SIGNAL(routeChanged()), this, SLOT(rebuildActions()));
    if (m_navigation)
        connect(m_navigation, SIGNAL(planChanged()), this, SLOT(rebuildActions()));
    if (m_navigation)
        connect(m_navigation, SIGNAL(planStateChanged()), this, SLOT(rebuildActions()));
    if (m_settings) {
        connect(m_settings, &SettingsStore::mapTypeChanged,
                this, &ShortcutMenuStore::rebuildActions);
        connect(m_settings, &SettingsStore::developerModeChanged,
                this, &ShortcutMenuStore::rebuildActions);
    }

    if (m_repo) {
        connect(m_repo, &MdbRepository::connectionStateChanged,
                this, &ShortcutMenuStore::rebuildActions);
        m_inputSubscriptionId = m_repo->subscribe(
            QLatin1String(kInputEventsChannel),
            [this](const QString &, const QString &message) { onInputEvent(message); });
    }
    rebuildActions();
}

ShortcutMenuStore::~ShortcutMenuStore()
{
    if (m_repo && m_inputSubscriptionId != 0)
        m_repo->unsubscribe(QLatin1String(kInputEventsChannel), m_inputSubscriptionId);
}

void ShortcutMenuStore::show()
{
    if (!isReadyToDrive())
        return;

    rebuildActions();
    if (!m_visible) {
        m_visible = true;
        m_selectedIndex = 0;
        m_confirming = false;
        m_pendingAction.clear();
        emit visibleChanged();
        emit selectionChanged();
        emit confirmingChanged();
    }
}

void ShortcutMenuStore::hide()
{
    if (m_visible)
        resetState();
}

void ShortcutMenuStore::cycle()
{
    if (m_actions.isEmpty())
        return;
    m_selectedIndex = (m_selectedIndex + 1) % m_actions.size();
    emit selectionChanged();
}

void ShortcutMenuStore::confirm()
{
    if (!m_visible || m_actions.isEmpty() || m_selectedIndex >= m_actions.size())
        return;

    m_pendingAction = m_actions.at(m_selectedIndex).toMap();
    m_confirming = true;
    emit confirmingChanged();
    m_confirmTimer->start();
}

bool ShortcutMenuStore::isReadyToDrive() const
{
    return m_vehicle
        && m_vehicle->state() == static_cast<int>(ScootEnums::VehicleState::ReadyToDrive);
}

bool ShortcutMenuStore::isStationary() const
{
    return m_engine && m_engine->hasSpeed() && std::abs(m_engine->speed()) <= 0.01
        && m_repo && m_repo->isConnected();
}

bool ShortcutMenuStore::destinationAvailable() const
{
    if (!isStationary() || !m_navigationAvailability || !m_settings)
        return false;
    const bool displayAvailable = m_navigationAvailability->property("localDisplayMapsAvailable").toBool()
        || m_settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
    return displayAvailable
        && m_navigationAvailability->property("routingAvailable").toBool();
}

QString ShortcutMenuStore::actionKey(const QVariantMap &action)
{
    const QString kind = action.value(QStringLiteral("kind")).toString();
    return kind == QLatin1String("destination")
        ? kind + QLatin1Char(':') + action.value(QStringLiteral("id")).toString()
        : kind;
}

QVariantList ShortcutMenuStore::availableActions() const
{
    QVariantList actions;
    if (m_settings && m_settingsService)
        actions.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("theme")}});
    actions.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("view")}});
    if (m_settings && m_settings->developerMode()) {
        actions.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("debug-overlay")}});
        actions.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("motion-debug")}});
    }
    if (m_navigation && m_navigation->property("hasRoute").toBool()) {
        actions.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("route-overview")}});
        // Skip is only meaningful while there is a later stop to move to.
        const int step = m_navigation->property("currentStep").toInt();
        const int count = m_navigation->property("stopCount").toInt();
        if (count > 0 && step + 1 < count)
            actions.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("skip-stop")}});
        actions.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("stop-navigation")}});
        return actions;
    }

    if (!m_savedLocations || !destinationAvailable())
        return actions;

    QList<QVariantMap> destinations;
    bool slotUsed[] = {false, false, false};
    for (const QVariant &value : m_savedLocations->property("locations").toList()) {
        QVariantMap location = value.toMap();
        const int slot = location.value(QStringLiteral("quickSlot")).toInt();
        if (slot < 1 || slot > 2 || slotUsed[slot])
            continue;
        slotUsed[slot] = true;
        if (location.value(QStringLiteral("label")).toString().isEmpty()) {
            location[QStringLiteral("label")] = QStringLiteral("%1, %2")
                .arg(location.value(QStringLiteral("latitude")).toDouble(), 0, 'f', 5)
                .arg(location.value(QStringLiteral("longitude")).toDouble(), 0, 'f', 5);
        }
        location[QStringLiteral("kind")] = QStringLiteral("destination");
        destinations.append(location);
    }
    std::sort(destinations.begin(), destinations.end(), [](const QVariantMap &left,
                                                            const QVariantMap &right) {
        return left.value(QStringLiteral("quickSlot")).toInt()
             < right.value(QStringLiteral("quickSlot")).toInt();
    });
    for (const auto &destination : destinations)
        actions.append(destination);
    return actions;
}

void ShortcutMenuStore::rebuildActions()
{
    const QVariantList next = availableActions();
    if (m_confirming) {
        bool pendingStillValid = false;
        for (const QVariant &value : next) {
            if (value.toMap() == m_pendingAction) {
                pendingStillValid = true;
                break;
            }
        }
        if (!pendingStillValid) {
            resetState();
            m_actions = next;
            emit actionsChanged();
            return;
        }
    }

    QString selectedKey;
    if (m_selectedIndex >= 0 && m_selectedIndex < m_actions.size())
        selectedKey = actionKey(m_actions.at(m_selectedIndex).toMap());
    if (next == m_actions)
        return;

    m_actions = next;
    int nextIndex = 0;
    for (int i = 0; i < m_actions.size(); ++i) {
        if (actionKey(m_actions.at(i).toMap()) == selectedKey) {
            nextIndex = i;
            break;
        }
    }
    const bool selectionMoved = nextIndex != m_selectedIndex;
    m_selectedIndex = nextIndex;
    emit actionsChanged();
    if (selectionMoved)
        emit selectionChanged();
}

void ShortcutMenuStore::onInputEvent(const QString &message)
{
    const QStringList parts = message.split(':');
    if (parts.size() != 2 || parts[0] != QLatin1String("seatbox"))
        return;
    if (!isReadyToDrive())
        return;

    const QString &gesture = parts[1];
    if (gesture == QLatin1String("long-tap")) {
        if (!m_visible) {
            show();
            m_cycleTimer->start();
        }
    } else if (gesture == QLatin1String("release")) {
        if (m_visible && !m_confirming) {
            m_cycleTimer->stop();
            confirm();
        }
    } else if (gesture == QLatin1String("press")) {
        if (m_confirming)
            executePendingAction();
    } else if (gesture == QLatin1String("double-tap")) {
        if (!m_visible && !m_confirming)
            toggleHazards();
    }
}

void ShortcutMenuStore::onCycleTimeout()
{
    cycle();
}

void ShortcutMenuStore::executePendingAction()
{
    const QVariantList current = availableActions();
    bool valid = false;
    for (const QVariant &value : current) {
        if (value.toMap() == m_pendingAction) {
            valid = true;
            break;
        }
    }
    if (!valid) {
        resetState();
        return;
    }

    const QString kind = m_pendingAction.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("theme")) {
        if (m_settings->theme() == QLatin1String("auto"))
            m_settingsService->updateTheme(QStringLiteral("dark"));
        else if (m_settings->theme() == QLatin1String("dark"))
            m_settingsService->updateTheme(QStringLiteral("light"));
        else
            m_settingsService->updateTheme(QStringLiteral("auto"));
        resetState();
        return;
    }
    if (kind == QLatin1String("view")) {
        toggleView();
        resetState();
        return;
    }
    if (kind == QLatin1String("debug-overlay")) {
        toggleDebugOverlay();
        resetState();
        return;
    }
    if (kind == QLatin1String("motion-debug")) {
        showMotionDebug();
        resetState();
        return;
    }
    if (kind == QLatin1String("route-overview")) {
        showRouteOverview();
        resetState();
        return;
    }
    if (kind == QLatin1String("skip-stop")) {
        skipCurrentStop();
        resetState();
        return;
    }
    if (kind == QLatin1String("stop-navigation")) {
        stopNavigation();
        resetState();
        return;
    }

    const int id = m_pendingAction.value(QStringLiteral("id")).toInt();
    QMetaObject::invokeMethod(m_savedLocations, "navigateToLocation", Q_ARG(int, id));
    resetState();
    if (m_screenStore)
        m_screenStore->setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
    if (m_settingsService)
        m_settingsService->updateMode(QStringLiteral("navigation"));
}

void ShortcutMenuStore::toggleHazards()
{
    if (!m_repo || !m_vehicle)
        return;
    const bool isBoth = m_vehicle->blinkerState()
        == static_cast<int>(ScootEnums::BlinkerState::Both);
    m_repo->push(QStringLiteral("scooter:blinker"),
                 isBoth ? QStringLiteral("off") : QStringLiteral("both"));
}

void ShortcutMenuStore::toggleDebugOverlay()
{
    if (!m_repo)
        return;
    const QString current = m_repo->get(QStringLiteral("dashboard"), QStringLiteral("debug"));
    m_repo->set(QStringLiteral("dashboard"), QStringLiteral("debug"),
                current == QLatin1String("overlay") ? QStringLiteral("off")
                                                    : QStringLiteral("overlay"));
}

void ShortcutMenuStore::showMotionDebug()
{
    if (m_screenStore)
        m_screenStore->setScreen(static_cast<int>(ScootEnums::ScreenMode::MotionDebug));
    if (m_settingsService)
        m_settingsService->updateMode(QStringLiteral("motion-debug"));
}

void ShortcutMenuStore::toggleView()
{
    if (!m_screenStore)
        return;

    const ScootEnums::ScreenMode current = m_screenStore->currentScreenMode();
    if (current == ScootEnums::ScreenMode::Cluster) {
        m_screenStore->setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
        if (m_settingsService)
            m_settingsService->updateMode(QStringLiteral("navigation"));
    } else if (current == ScootEnums::ScreenMode::Map) {
        m_screenStore->setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
        if (m_settingsService)
            m_settingsService->updateMode(QStringLiteral("speedometer"));
    }
}

void ShortcutMenuStore::stopNavigation()
{
    if (m_navigation && m_navigation->property("hasRoute").toBool())
        QMetaObject::invokeMethod(m_navigation, "clearNavigation");
}

void ShortcutMenuStore::skipCurrentStop()
{
    if (!m_navigation || !m_navigation->property("hasPlan").toBool())
        return;
    QMetaObject::invokeMethod(m_navigation, "skipCurrentStop");
    if (m_screenStore)
        m_screenStore->setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
    if (m_settingsService)
        m_settingsService->updateMode(QStringLiteral("navigation"));
}

void ShortcutMenuStore::showRouteOverview()
{
    if (!m_navigation || !m_navigation->property("hasRoute").toBool() || !m_mapService)
        return;
    bool shown = false;
    QMetaObject::invokeMethod(m_mapService, "showRouteOverview", Q_RETURN_ARG(bool, shown));
    if (!shown)
        return;
    if (m_screenStore)
        m_screenStore->setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
    if (m_settingsService)
        m_settingsService->updateMode(QStringLiteral("navigation"));
}

void ShortcutMenuStore::resetState()
{
    m_cycleTimer->stop();
    m_confirmTimer->stop();

    const bool wasVisible = m_visible;
    const bool wasConfirming = m_confirming;
    const bool selectionMoved = m_selectedIndex != 0;
    m_visible = false;
    m_confirming = false;
    m_selectedIndex = 0;
    m_pendingAction.clear();

    if (wasVisible)
        emit visibleChanged();
    if (wasConfirming)
        emit confirmingChanged();
    if (selectionMoved)
        emit selectionChanged();
}

#include "Navigator.h"
#include "stores/SettingsStore.h"
#include "stores/VehicleStore.h"
#include "commands/CommandBus.h"

Navigator::Navigator(SettingsStore *settings, CommandBus *commands, QObject *parent)
    : QObject(parent)
    , m_commands(commands)
{
    applyMode(settings->mode());
    connect(settings, &SettingsStore::modeChanged, this, [this, settings]() {
        applyMode(settings->mode());
    });
}

bool Navigator::isBrakeNavigated(ScootEnums::ScreenMode mode)
{
    switch (mode) {
    case ScootEnums::ScreenMode::About:
    case ScootEnums::ScreenMode::AddressSelection:
    case ScootEnums::ScreenMode::NavigationSetup:
    case ScootEnums::ScreenMode::Faults:
    case ScootEnums::ScreenMode::SystemInfo:
    case ScootEnums::ScreenMode::UpdateModeInfo:
    case ScootEnums::ScreenMode::UpdateChannel:
    case ScootEnums::ScreenMode::HopOnInfo:
        return true;
    default:
        return false;
    }
}

void Navigator::publishMenuOpen()
{
    if (!m_commands) return;
    m_commands->setMenuOpen(isBrakeNavigated(m_currentScreen));
}

void Navigator::applyMode(const QString &mode)
{
    ScootEnums::ScreenMode target = ScootEnums::ScreenMode::Cluster;
    if (mode == QLatin1String("navigation"))
        target = ScootEnums::ScreenMode::Map;
    else if (mode == QLatin1String("debug"))
        target = ScootEnums::ScreenMode::Debug;
    else if (mode == QLatin1String("motion-debug"))
        target = ScootEnums::ScreenMode::MotionDebug;

    if (target != m_currentScreen) {
        m_currentScreen = target;
        publishMenuOpen();
        emit currentScreenChanged();
    }
}

void Navigator::setScreen(int screen)
{
    auto mode = static_cast<ScootEnums::ScreenMode>(screen);
    if (mode != m_currentScreen) {
        m_currentScreen = mode;
        publishMenuOpen();
        emit currentScreenChanged();
    }
}

void Navigator::showAddressSelection()
{
    m_screenBeforeAddressSelection = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::AddressSelection));
}

// Cancelling out. Confirming a destination hands over to the map instead and
// does not come through here.
void Navigator::closeAddressSelection()
{
    setScreen(static_cast<int>(m_screenBeforeAddressSelection));
}

void Navigator::showAbout()
{
    m_screenBeforeAbout = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::About));
}

void Navigator::closeAbout()
{
    setScreen(static_cast<int>(m_screenBeforeAbout));
}

void Navigator::showNavigationSetup(int setupMode)
{
    m_screenBeforeNavSetup = m_currentScreen;
    if (setupMode != m_setupMode) {
        m_setupMode = setupMode;
        emit setupModeChanged();
    }
    setScreen(static_cast<int>(ScootEnums::ScreenMode::NavigationSetup));
}

void Navigator::closeNavigationSetup()
{
    setScreen(static_cast<int>(m_screenBeforeNavSetup));
}

void Navigator::showFaults()
{
    m_screenBeforeFaults = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::Faults));
}

void Navigator::closeFaults()
{
    setScreen(static_cast<int>(m_screenBeforeFaults));
}

void Navigator::showSystemInfo(int page)
{
    if (page != m_systemInfoPage) {
        m_systemInfoPage = page;
        emit systemInfoPageChanged();
    }
    m_screenBeforeSystemInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::SystemInfo));
}

void Navigator::closeSystemInfo()
{
    setScreen(static_cast<int>(m_screenBeforeSystemInfo));
}

void Navigator::showUpdateModeInfo()
{
    m_screenBeforeUpdateModeInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::UpdateModeInfo));
}

void Navigator::closeUpdateModeInfo()
{
    setScreen(static_cast<int>(m_screenBeforeUpdateModeInfo));
}

void Navigator::confirmUpdateMode()
{
    emit umsModeRequested();
    setScreen(static_cast<int>(m_screenBeforeUpdateModeInfo));
}

void Navigator::showUpdateChannel()
{
    m_screenBeforeUpdateChannel = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::UpdateChannel));
}

void Navigator::closeUpdateChannel()
{
    setScreen(static_cast<int>(m_screenBeforeUpdateChannel));
}

void Navigator::showHopOnInfo()
{
    m_screenBeforeHopOnInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::HopOnInfo));
}

void Navigator::closeHopOnInfo()
{
    setScreen(static_cast<int>(m_screenBeforeHopOnInfo));
}

void Navigator::enterHopOnLock()
{
    m_screenBeforeHopOnLock = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
}

void Navigator::exitHopOnLock()
{
    setScreen(static_cast<int>(m_screenBeforeHopOnLock));
}

void Navigator::attachVehicle(VehicleStore *vehicle)
{
    connect(vehicle, &VehicleStore::stateChanged, this, [this, vehicle]() {
        if (vehicle->state() != static_cast<int>(ScootEnums::VehicleState::ReadyToDrive))
            return;
        switch (m_currentScreen) {
        case ScootEnums::ScreenMode::About:      closeAbout(); break;
        case ScootEnums::ScreenMode::Faults:     closeFaults(); break;
        case ScootEnums::ScreenMode::SystemInfo: closeSystemInfo(); break;
        default: break;
        }
    });
}

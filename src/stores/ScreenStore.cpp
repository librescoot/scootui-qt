#include "ScreenStore.h"
#include "SettingsStore.h"
#include "../repositories/MdbRepository.h"


ScreenStore::ScreenStore(SettingsStore *settings, MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent), m_settings(settings)
{
    applyMode(settings->mode());
    connect(settings, &SettingsStore::modeChanged, this, [this, settings]() {
        applyMode(settings->mode());
    });
    connect(settings, &SettingsStore::developerModeChanged, this, [this, settings]() {
        applyMode(settings->mode());
    });
    connect(settings, &SettingsStore::otaChannelChanged, this, [this, settings]() {
        applyMode(settings->mode());
    });
}

bool ScreenStore::isBrakeNavigated(ScootEnums::ScreenMode mode)
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
    case ScootEnums::ScreenMode::KeycardEnrollInfo:
        return true;
    default:
        return false;
    }
}

void ScreenStore::closeParkedScreens()
{
    // isBrakeNavigated() is the parked-only set, the same screens the menu
    // mirrors into dashboard:menu-open.
    if (!isBrakeNavigated(m_currentScreen))
        return;

    // AddressSelection is the one overlay holding state beyond the screen.
    if (m_addressSelectionAppend) {
        m_addressSelectionAppend = false;
        emit addressSelectionAppendChanged();
    }

    // Overlays can be opened from other overlays, so m_screenBeforeX is not
    // always somewhere a rider should be left; the main screen always is.
    setScreen(static_cast<int>(m_mainScreen));
}

void ScreenStore::publishMenuOpen()
{
    if (!m_repo) return;
    m_repo->set(QStringLiteral("dashboard"), QStringLiteral("menu-open"),
                isBrakeNavigated(m_currentScreen) ? QStringLiteral("true")
                                                 : QStringLiteral("false"));
}

void ScreenStore::applyScreenLocally(ScootEnums::ScreenMode mode)
{
    if (mode == ScootEnums::ScreenMode::MotionDebug
        && (!m_settings || !m_settings->debugActionsAllowed()))
        return;
    if (mode == m_currentScreen) return;
    m_currentScreen = mode;
    if (!isBrakeNavigated(mode))
        m_mainScreen = mode;
    publishMenuOpen();
    emit currentScreenChanged();
}

SyncSettings ScreenStore::syncSettings() const
{
    return {QStringLiteral("dashboard"), 500, {}, {}, {}};
}

void ScreenStore::applyFieldUpdate(const QString &, const QString &)
{
}

void ScreenStore::applyMode(const QString &mode)
{
    ScootEnums::ScreenMode target = ScootEnums::ScreenMode::Cluster;
    if (mode == QLatin1String("navigation"))
        target = ScootEnums::ScreenMode::Map;
    else if (mode == QLatin1String("debug"))
        target = ScootEnums::ScreenMode::Debug;
    else if (mode == QLatin1String("motion-debug")
             && m_settings && m_settings->debugActionsAllowed())
        target = ScootEnums::ScreenMode::MotionDebug;

    setScreen(static_cast<int>(target));
}

void ScreenStore::setScreen(int screen)
{
    auto mode = static_cast<ScootEnums::ScreenMode>(screen);
    if (mode == m_currentScreen) return;

    applyScreenLocally(mode);
}

void ScreenStore::showAddressSelection(bool appendToRoute)
{
    m_screenBeforeAddressSelection = m_currentScreen;
    if (m_addressSelectionAppend != appendToRoute) {
        m_addressSelectionAppend = appendToRoute;
        emit addressSelectionAppendChanged();
    }
    setScreen(static_cast<int>(ScootEnums::ScreenMode::AddressSelection));
}

// Cancelling out. Confirming a destination hands over to the map instead and
// does not come through here.
void ScreenStore::closeAddressSelection()
{
    if (m_addressSelectionAppend) {
        m_addressSelectionAppend = false;
        emit addressSelectionAppendChanged();
    }
    setScreen(static_cast<int>(m_screenBeforeAddressSelection));
}

void ScreenStore::showAbout()
{
    m_screenBeforeAbout = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::About));
}

void ScreenStore::closeAbout()
{
    setScreen(static_cast<int>(m_screenBeforeAbout));
}

void ScreenStore::showNavigationSetup(int setupMode)
{
    m_screenBeforeNavSetup = m_currentScreen;
    if (setupMode != m_setupMode) {
        m_setupMode = setupMode;
        emit setupModeChanged();
    }
    setScreen(static_cast<int>(ScootEnums::ScreenMode::NavigationSetup));
}

void ScreenStore::closeNavigationSetup()
{
    setScreen(static_cast<int>(m_screenBeforeNavSetup));
}

void ScreenStore::showFaults()
{
    m_screenBeforeFaults = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::Faults));
}

void ScreenStore::closeFaults()
{
    setScreen(static_cast<int>(m_screenBeforeFaults));
}

void ScreenStore::showSystemInfo(int page)
{
    if (page != m_systemInfoPage) {
        m_systemInfoPage = page;
        emit systemInfoPageChanged();
    }
    m_screenBeforeSystemInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::SystemInfo));
}

void ScreenStore::closeSystemInfo()
{
    setScreen(static_cast<int>(m_screenBeforeSystemInfo));
}

void ScreenStore::showUpdateModeInfo()
{
    m_screenBeforeUpdateModeInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::UpdateModeInfo));
}

void ScreenStore::closeUpdateModeInfo()
{
    setScreen(static_cast<int>(m_screenBeforeUpdateModeInfo));
}

void ScreenStore::confirmUpdateMode()
{
    emit umsModeRequested();
    setScreen(static_cast<int>(m_screenBeforeUpdateModeInfo));
}

void ScreenStore::showUpdateChannel()
{
    m_screenBeforeUpdateChannel = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::UpdateChannel));
}

void ScreenStore::closeUpdateChannel()
{
    setScreen(static_cast<int>(m_screenBeforeUpdateChannel));
}

void ScreenStore::showHopOnInfo()
{
    m_screenBeforeHopOnInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::HopOnInfo));
}

void ScreenStore::closeHopOnInfo()
{
    setScreen(static_cast<int>(m_screenBeforeHopOnInfo));
}

void ScreenStore::showKeycardEnrollInfo()
{
    if (m_currentScreen != ScootEnums::ScreenMode::KeycardEnrollInfo)
        m_screenBeforeKeycardEnrollInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::KeycardEnrollInfo));
}

void ScreenStore::closeKeycardEnrollInfo()
{
    setScreen(static_cast<int>(m_screenBeforeKeycardEnrollInfo));
}

void ScreenStore::enterHopOnLock()
{
    m_screenBeforeHopOnLock = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
}

void ScreenStore::exitHopOnLock()
{
    setScreen(static_cast<int>(m_screenBeforeHopOnLock));
}

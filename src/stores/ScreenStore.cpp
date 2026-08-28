#include "ScreenStore.h"
#include "SettingsStore.h"
#include "../repositories/MdbRepository.h"

ScreenStore::ScreenStore(SettingsStore *settings, MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
    applyMode(settings->mode());
    connect(settings, &SettingsStore::modeChanged, this, [this, settings]() {
        applyMode(settings->mode());
    });
    // modeChanged alone is not enough to keep the screen on the key. It is an
    // edge on the value, and SyncableStore re-reads the whole hash after a
    // pub/sub ping rather than taking the value from the payload, so a write
    // that is reverted before that read lands is never seen here. An overlaid
    // key does exactly that: settings-service re-asserts the overlay value in
    // the same callback that saw the edit. Leaving the debug screen in service
    // mode used to strand the dashboard on the cluster with dashboard.mode
    // still reading debug and nothing left to correct it.
    connect(settings, &SettingsStore::settingsRefreshed, this, [this, settings]() {
        resyncMode(settings->mode());
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
        return true;
    default:
        return false;
    }
}

void ScreenStore::publishMenuOpen()
{
    if (!m_repo) return;
    m_repo->set(QStringLiteral("dashboard"), QStringLiteral("menu-open"),
                isBrakeNavigated(m_currentScreen) ? QStringLiteral("true")
                                                 : QStringLiteral("false"));
}

void ScreenStore::applyMode(const QString &mode)
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

// Level-triggered counterpart to applyMode: called on every refresh of the
// settings hash, so a screen that drifted from the key finds its way back
// within one poll interval.
//
// Only the four mode-backed screens are corrected. A brake-navigated screen is
// a deliberate detour that remembers what it covered and restores it on close,
// and the hop-on lock parks on the cluster on purpose; pulling either back to
// the key would undo a screen the user is looking at.
void ScreenStore::resyncMode(const QString &mode)
{
    if (m_hopOnLockActive || isBrakeNavigated(m_currentScreen))
        return;
    applyMode(mode);
}

void ScreenStore::setScreen(int screen)
{
    auto mode = static_cast<ScootEnums::ScreenMode>(screen);
    if (mode != m_currentScreen) {
        m_currentScreen = mode;
        publishMenuOpen();
        emit currentScreenChanged();
    }
}

void ScreenStore::showAddressSelection()
{
    m_screenBeforeAddressSelection = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::AddressSelection));
}

// Cancelling out. Confirming a destination hands over to the map instead and
// does not come through here.
void ScreenStore::closeAddressSelection()
{
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

void ScreenStore::enterHopOnLock()
{
    m_screenBeforeHopOnLock = m_currentScreen;
    m_hopOnLockActive = true;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
}

void ScreenStore::exitHopOnLock()
{
    m_hopOnLockActive = false;
    setScreen(static_cast<int>(m_screenBeforeHopOnLock));
}

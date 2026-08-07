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
}

bool ScreenStore::isBrakeNavigated(ScootEnums::ScreenMode mode)
{
    switch (mode) {
    case ScootEnums::ScreenMode::About:
    case ScootEnums::ScreenMode::AddressSelection:
    case ScootEnums::ScreenMode::NavigationSetup:
    case ScootEnums::ScreenMode::Destination:
    case ScootEnums::ScreenMode::Faults:
    case ScootEnums::ScreenMode::SystemInfo:
    case ScootEnums::ScreenMode::UpdateModeInfo:
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

void ScreenStore::setScreen(int screen)
{
    auto mode = static_cast<ScootEnums::ScreenMode>(screen);
    if (mode != m_currentScreen) {
        m_currentScreen = mode;
        publishMenuOpen();
        emit currentScreenChanged();
    }
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
    setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
}

void ScreenStore::exitHopOnLock()
{
    setScreen(static_cast<int>(m_screenBeforeHopOnLock));
}

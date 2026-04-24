#include "ScreenStore.h"
#include "SettingsStore.h"

ScreenStore::ScreenStore(SettingsStore *settings, QObject *parent)
    : QObject(parent)
{
    applyMode(settings->mode());
    connect(settings, &SettingsStore::modeChanged, this, [this, settings]() {
        applyMode(settings->mode());
    });
}

void ScreenStore::applyMode(const QString &mode)
{
    ScootEnums::ScreenMode target = ScootEnums::ScreenMode::Cluster;
    if (mode == QLatin1String("navigation"))
        target = ScootEnums::ScreenMode::Map;
    else if (mode == QLatin1String("debug"))
        target = ScootEnums::ScreenMode::Debug;

    if (target != m_currentScreen) {
        m_currentScreen = target;
        emit currentScreenChanged();
    }
}

void ScreenStore::setScreen(int screen)
{
    auto mode = static_cast<ScootEnums::ScreenMode>(screen);
    if (mode != m_currentScreen) {
        m_currentScreen = mode;
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

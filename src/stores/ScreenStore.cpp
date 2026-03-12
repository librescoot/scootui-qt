#include "ScreenStore.h"

ScreenStore::ScreenStore(QObject *parent)
    : QObject(parent)
{
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

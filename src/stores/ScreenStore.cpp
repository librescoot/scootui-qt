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

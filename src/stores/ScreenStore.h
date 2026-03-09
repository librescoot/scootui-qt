#pragma once

#include <QObject>
#include "models/Enums.h"

class ScreenStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentScreen READ currentScreen NOTIFY currentScreenChanged)

public:
    explicit ScreenStore(QObject *parent = nullptr);

    int currentScreen() const { return static_cast<int>(m_currentScreen); }
    ScootEnums::ScreenMode currentScreenMode() const { return m_currentScreen; }

    Q_INVOKABLE void setScreen(int screen);

signals:
    void currentScreenChanged();

private:
    ScootEnums::ScreenMode m_currentScreen = ScootEnums::ScreenMode::Cluster;
};

#pragma once

#include <QObject>
#include "models/Enums.h"

class SettingsStore;

class ScreenStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentScreen READ currentScreen NOTIFY currentScreenChanged)

public:
    explicit ScreenStore(SettingsStore *settings, QObject *parent = nullptr);

    int currentScreen() const { return static_cast<int>(m_currentScreen); }
    ScootEnums::ScreenMode currentScreenMode() const { return m_currentScreen; }

    Q_INVOKABLE void setScreen(int screen);
    Q_INVOKABLE void showAbout();
    Q_INVOKABLE void closeAbout();

signals:
    void currentScreenChanged();

private:
    void applyMode(const QString &mode);

    ScootEnums::ScreenMode m_currentScreen = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeAbout = ScootEnums::ScreenMode::Cluster;
};

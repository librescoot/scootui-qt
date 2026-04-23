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

    Q_PROPERTY(int setupMode READ setupMode NOTIFY setupModeChanged)

    Q_INVOKABLE void setScreen(int screen);
    Q_INVOKABLE void showAbout();
    Q_INVOKABLE void closeAbout();
    Q_INVOKABLE void showNavigationSetup(int setupMode = 2);
    Q_INVOKABLE void closeNavigationSetup();
    Q_INVOKABLE void showFaults();
    Q_INVOKABLE void closeFaults();

    int setupMode() const { return m_setupMode; }

signals:
    void currentScreenChanged();
    void setupModeChanged();

private:
    void applyMode(const QString &mode);

    ScootEnums::ScreenMode m_currentScreen = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeAbout = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeNavSetup = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeFaults = ScootEnums::ScreenMode::Cluster;
    int m_setupMode = 2; // Both by default
};

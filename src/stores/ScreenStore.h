#pragma once

#include <QObject>
#include "models/Enums.h"

class SettingsStore;
class MdbRepository;

class ScreenStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentScreen READ currentScreen NOTIFY currentScreenChanged)

public:
    explicit ScreenStore(SettingsStore *settings, MdbRepository *repo,
                         QObject *parent = nullptr);

    // Brake-navigated screens drive their own UI via brake-lever taps; while
    // any of them is up we mirror MenuStore's dashboard:menu-open=true so
    // vehicle-service suppresses brake-light LED cues for the navigation taps.
    static bool isBrakeNavigated(ScootEnums::ScreenMode mode);

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
    Q_INVOKABLE void showUpdateModeInfo();
    Q_INVOKABLE void closeUpdateModeInfo();
    // Confirms UMS entry: emits umsModeRequested (handled in Application
    // which owns the repo pointer) and closes the info screen.
    Q_INVOKABLE void confirmUpdateMode();
    Q_INVOKABLE void showHopOnInfo();
    Q_INVOKABLE void closeHopOnInfo();

    // Hop-on Locked: switch to Cluster (lightweight) so the heavy underlying
    // screen (notably MapScreen with QtLocation) doesn't keep rendering under
    // the opaque lock overlay. Restored to whatever the user was on when the
    // matcher accepts the unlock combo.
    Q_INVOKABLE void enterHopOnLock();
    Q_INVOKABLE void exitHopOnLock();

    int setupMode() const { return m_setupMode; }

signals:
    void currentScreenChanged();
    void setupModeChanged();
    void umsModeRequested();

private:
    void applyMode(const QString &mode);
    void publishMenuOpen();

    MdbRepository *m_repo;
    ScootEnums::ScreenMode m_currentScreen = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeAbout = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeNavSetup = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeFaults = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeUpdateModeInfo = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeHopOnInfo = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeHopOnLock = ScootEnums::ScreenMode::Cluster;
    int m_setupMode = 2; // Both by default
};

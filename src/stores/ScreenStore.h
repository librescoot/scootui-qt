#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"

class SettingsStore;

class ScreenStore : public SyncableStore
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
    Q_PROPERTY(int systemInfoPage READ systemInfoPage NOTIFY systemInfoPageChanged)

    Q_INVOKABLE void setScreen(int screen);
    Q_INVOKABLE void showAddressSelection();
    Q_INVOKABLE void closeAddressSelection();
    Q_INVOKABLE void showAbout();
    Q_INVOKABLE void closeAbout();
    Q_INVOKABLE void showNavigationSetup(int setupMode = 2);
    Q_INVOKABLE void closeNavigationSetup();
    Q_INVOKABLE void showFaults();
    Q_INVOKABLE void closeFaults();
    // page selects which System Info section is shown; see SystemInfoPage.
    Q_INVOKABLE void showSystemInfo(int page = 0);
    Q_INVOKABLE void closeSystemInfo();
    Q_INVOKABLE void showUpdateModeInfo();
    Q_INVOKABLE void closeUpdateModeInfo();
    // Confirms UMS entry: emits umsModeRequested (handled in Application
    // which owns the repo pointer) and closes the info screen.
    Q_INVOKABLE void confirmUpdateMode();
    // Channel-switch confirmation. The switch itself lives in
    // UpdateChannelService; this only owns which screen is up.
    Q_INVOKABLE void showUpdateChannel();
    Q_INVOKABLE void closeUpdateChannel();
    Q_INVOKABLE void showHopOnInfo();
    Q_INVOKABLE void closeHopOnInfo();

    // Hop-on Locked: switch to Cluster (lightweight) so the heavy underlying
    // screen (notably MapScreen with QtLocation) doesn't keep rendering under
    // the opaque lock overlay. Restored to whatever the user was on when the
    // matcher accepts the unlock combo.
    Q_INVOKABLE void enterHopOnLock();
    Q_INVOKABLE void exitHopOnLock();

    int setupMode() const { return m_setupMode; }
    int systemInfoPage() const { return m_systemInfoPage; }

    enum SystemInfoPage { SystemInfoDevice = 0, SystemInfoConnectivity = 1, SystemInfoBatteries = 2,
                          SystemInfoMaps = 3 };
    Q_ENUM(SystemInfoPage)

signals:
    void currentScreenChanged();
    void setupModeChanged();
    void systemInfoPageChanged();
    void umsModeRequested();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    void applyMode(const QString &mode);
    void publishMenuOpen();
    // Mirrors the current screen into the "dashboard" hash (remote-screen)
    // so any other scootui-qt instance sharing this Redis - a remote viewer,
    // or the simulator panel driving a real bench unit - sees and can steer
    // the same screen. Distinct from settings.dashboard.mode, which is the
    // persisted boot-time preference (applyMode() below).
    void publishScreen();
    // Applies a screen transition without touching Redis. Shared by
    // setScreen() (publishes afterwards) and applyFieldUpdate() (reacting to
    // a value already in Redis, so publishing it back would just echo).
    void applyScreenLocally(ScootEnums::ScreenMode mode);
    static QString screenName(ScootEnums::ScreenMode mode);
    static bool screenModeFromName(const QString &name, ScootEnums::ScreenMode &out);

    ScootEnums::ScreenMode m_currentScreen = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeAddressSelection = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeAbout = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeNavSetup = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeFaults = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeSystemInfo = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeUpdateModeInfo = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeUpdateChannel = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeHopOnInfo = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeHopOnLock = ScootEnums::ScreenMode::Cluster;
    int m_setupMode = 2; // Both by default
    int m_systemInfoPage = SystemInfoDevice;
};

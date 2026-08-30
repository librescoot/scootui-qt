#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <memory>

class MenuNode;
class SettingsStore;
class VehicleStore;
class ThemeStore;
class Translations;
class SettingsService;
class CommandBus;
class NavigationService;
class SavedLocationsStore;
class RecentDestinationsStore;
class ScreenStore;
class NavigationAvailabilityService;
class InternetStore;
class HopOnStore;
class MapDownloadService;
class FaultsService;
class ToastService;
class UpdateChannelService;

class MenuStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY isOpenChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY menuChanged)
    Q_PROPERTY(QVariantList currentItems READ currentItems NOTIFY menuChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY menuChanged)
    Q_PROPERTY(bool isRoot READ isRoot NOTIFY menuChanged)
    Q_PROPERTY(QString parentTitle READ parentTitle NOTIFY menuChanged)
    // True when the level has somewhere else to go, which is any level with
    // more than one row. Not a direction: the list wraps, so there is no end
    // to be at, and the two directional predicates this replaces were the
    // same expression under names that promised a position check neither did.
    Q_PROPERTY(bool canScroll READ canScroll NOTIFY menuChanged)
    // The action a right long-tap would run on the selected row, named
    // so the hint bar can print it. Empty when the row has no shortcut.
    Q_PROPERTY(QString selectedPrimaryLabel READ selectedPrimaryLabel NOTIFY menuChanged)

public:
    explicit MenuStore(SettingsStore *settings, VehicleStore *vehicle,
                       ThemeStore *theme,
                       Translations *translations, SettingsService *settingsService,
                       CommandBus *commands, QObject *parent = nullptr);

    void setNavigationService(NavigationService *svc);
    void setSavedLocationsStore(SavedLocationsStore *store);
    void setRecentDestinationsStore(RecentDestinationsStore *store);
    void setScreenStore(ScreenStore *store);
    void setNavigationAvailabilityService(NavigationAvailabilityService *svc);
    void setInternetStore(InternetStore *store);
    void setHopOnStore(HopOnStore *store);
    void setMapDownloadService(MapDownloadService *svc);
    void setFaultsService(FaultsService *svc);
    void setToastService(ToastService *svc);
    void setUpdateChannelService(UpdateChannelService *svc);
    ~MenuStore() override;

    bool isOpen() const { return m_isOpen; }
    QString currentTitle() const;
    QVariantList currentItems() const;
    int selectedIndex() const { return m_selectedIndex; }
    bool isRoot() const { return m_pathStack.isEmpty(); }
    QString parentTitle() const;
    bool canScroll() const;
    QString selectedPrimaryLabel() const;

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    // Closes the menu to hand the display to a full-screen page, remembering
    // where in the tree we stood. resume() puts the rider back on that level
    // when the page is dismissed; any other close() drops the memory.
    void closeForScreen();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void navigateDown();
    Q_INVOKABLE void selectItem();
    Q_INVOKABLE void goBack();
    // Runs the selected row's primary child without entering it.
    Q_INVOKABLE void activatePrimary();

signals:
    void isOpenChanged();
    void menuChanged();

private:
    void openAt(const QStringList &path, const QList<int> &indexStack, int index);
    // Records which row the selection is on, by id. Call it whenever the
    // selection moves, never at rebuild time: visibility conditions are
    // evaluated live, so by the time a rebuild runs the list has already
    // taken the change that is about to shift the rows.
    void rememberSelection();
    void clearResume();
    void rebuildMenuTree();
    QString lastMapCheckLabel() const;
    QString lastCheckLabel(const QString &iso) const;
    MenuNode *findCurrentNode() const;
    MenuNode *selectedPrimaryNode() const;
    void emitMenuChanged();
    bool isRoutingReady() const;

    SettingsStore *m_settings;
    VehicleStore *m_vehicle;
    ThemeStore *m_theme;
    Translations *m_translations;
    SettingsService *m_settingsService;
    CommandBus *m_commands;
    NavigationService *m_navigationService = nullptr;
    SavedLocationsStore *m_savedLocations = nullptr;
    RecentDestinationsStore *m_recentDestinations = nullptr;
    ScreenStore *m_screenStore = nullptr;
    NavigationAvailabilityService *m_navAvailability = nullptr;
    InternetStore *m_internet = nullptr;
    HopOnStore *m_hopOn = nullptr;
    MapDownloadService *m_mapDownload = nullptr;
    FaultsService *m_faults = nullptr;
    ToastService *m_toastService = nullptr;
    UpdateChannelService *m_updateChannel = nullptr;

    std::unique_ptr<MenuNode> m_rootNode;
    bool m_isOpen = false;
    int m_selectedIndex = 0;
    QStringList m_pathStack;      // node IDs for navigation depth
    QList<int> m_indexStack;      // selected index at each depth

    // Where closeForScreen() left off, replayed by resume().
    QString m_selectedId;

    QStringList m_resumePath;
    QList<int> m_resumeIndexStack;
    int m_resumeIndex = 0;
    bool m_resumeArmed = false;
    bool m_executingAction = false; // guard against reentrant rebuilds

    // vehicle-service emits "tap" right before "double-tap" on a double-tap.
    // The trailing tap races with menu open and would shift selection off
    // the first item. Drop navigation input in the first few ms after open.
    QElapsedTimer m_openedAt;
    static constexpr qint64 kOpenInputGraceMs = 150;
};

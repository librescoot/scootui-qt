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
class TripStore;
class Translations;
class SettingsService;
class MdbRepository;
class NavigationService;
class SavedLocationsStore;
class RecentDestinationsStore;
class ScreenStore;
class NavigationAvailabilityService;
class InternetStore;
class HopOnStore;
class MapDownloadService;
class FaultsStore;
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
    Q_PROPERTY(bool canScrollUp READ canScrollUp NOTIFY menuChanged)
    Q_PROPERTY(bool canScrollDown READ canScrollDown NOTIFY menuChanged)

public:
    explicit MenuStore(SettingsStore *settings, VehicleStore *vehicle,
                       ThemeStore *theme, TripStore *trip,
                       Translations *translations, SettingsService *settingsService,
                       MdbRepository *repo, QObject *parent = nullptr);

    void setNavigationService(NavigationService *svc);
    void setSavedLocationsStore(SavedLocationsStore *store);
    void setRecentDestinationsStore(RecentDestinationsStore *store);
    void setScreenStore(ScreenStore *store);
    void setNavigationAvailabilityService(NavigationAvailabilityService *svc);
    void setInternetStore(InternetStore *store);
    void setHopOnStore(HopOnStore *store);
    void setMapDownloadService(MapDownloadService *svc);
    void setFaultsStore(FaultsStore *store);
    void setToastService(ToastService *svc);
    void setUpdateChannelService(UpdateChannelService *svc);
    ~MenuStore() override;

    bool isOpen() const { return m_isOpen; }
    QString currentTitle() const;
    QVariantList currentItems() const;
    int selectedIndex() const { return m_selectedIndex; }
    bool isRoot() const { return m_pathStack.isEmpty(); }
    bool canScrollUp() const;
    bool canScrollDown() const;

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    // Closes the menu to hand the display to a full-screen page, remembering
    // where in the tree we stood. resume() puts the rider back on that level
    // when the page is dismissed; any other close() drops the memory.
    void closeForScreen();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void navigateUp();
    Q_INVOKABLE void navigateDown();
    Q_INVOKABLE void selectItem();
    Q_INVOKABLE void goBack();

signals:
    void isOpenChanged();
    void menuChanged();

private:
    void openAt(const QStringList &path, const QList<int> &indexStack, int index);
    void clearResume();
    void rebuildMenuTree();
    QString lastMapCheckLabel() const;
    QString lastCheckLabel(const QString &iso) const;
    MenuNode *findCurrentNode() const;
    void emitMenuChanged();
    bool isRoutingReady() const;

    SettingsStore *m_settings;
    VehicleStore *m_vehicle;
    ThemeStore *m_theme;
    TripStore *m_trip;
    Translations *m_translations;
    SettingsService *m_settingsService;
    MdbRepository *m_repo;
    NavigationService *m_navigationService = nullptr;
    SavedLocationsStore *m_savedLocations = nullptr;
    RecentDestinationsStore *m_recentDestinations = nullptr;
    ScreenStore *m_screenStore = nullptr;
    NavigationAvailabilityService *m_navAvailability = nullptr;
    InternetStore *m_internet = nullptr;
    HopOnStore *m_hopOn = nullptr;
    MapDownloadService *m_mapDownload = nullptr;
    FaultsStore *m_faults = nullptr;
    ToastService *m_toastService = nullptr;
    UpdateChannelService *m_updateChannel = nullptr;

    std::unique_ptr<MenuNode> m_rootNode;
    bool m_isOpen = false;
    int m_selectedIndex = 0;
    QStringList m_pathStack;      // node IDs for navigation depth
    QList<int> m_indexStack;      // selected index at each depth

    // Where closeForScreen() left off, replayed by resume().
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

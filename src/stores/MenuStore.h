#pragma once

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
class ScreenStore;

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
    void setScreenStore(ScreenStore *store);
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
    Q_INVOKABLE void navigateUp();
    Q_INVOKABLE void navigateDown();
    Q_INVOKABLE void selectItem();
    Q_INVOKABLE void goBack();

signals:
    void isOpenChanged();
    void menuChanged();

private:
    void rebuildMenuTree();
    MenuNode *findCurrentNode() const;
    void emitMenuChanged();

    SettingsStore *m_settings;
    VehicleStore *m_vehicle;
    ThemeStore *m_theme;
    TripStore *m_trip;
    Translations *m_translations;
    SettingsService *m_settingsService;
    MdbRepository *m_repo;
    NavigationService *m_navigationService = nullptr;
    SavedLocationsStore *m_savedLocations = nullptr;
    ScreenStore *m_screenStore = nullptr;

    std::unique_ptr<MenuNode> m_rootNode;
    bool m_isOpen = false;
    int m_selectedIndex = 0;
    QStringList m_pathStack;      // node IDs for navigation depth
    QList<int> m_indexStack;      // selected index at each depth
};

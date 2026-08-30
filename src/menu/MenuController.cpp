#include "MenuController.h"
#include "MenuNode.h"
#include "MenuDefinition.h"
#include "stores/SettingsStore.h"
#include "stores/VehicleStore.h"
#include "stores/ThemeStore.h"
#include "core/Navigator.h"
#include "stores/SavedLocationsStore.h"
#include "stores/RecentDestinationsStore.h"
#include "stores/InternetStore.h"
#include "services/HopOnService.h"
#include "services/FaultsService.h"
#include "l10n/Translations.h"
#include "services/ToastService.h"
#include "services/SettingsService.h"
#include "services/NavigationService.h"
#include "services/NavigationAvailabilityService.h"
#include "services/MapDownloadService.h"
#include "services/UpdateChannelService.h"
#include "commands/CommandBus.h"

#include <QDebug>

MenuController::MenuController(SettingsStore *settings, VehicleStore *vehicle,
                               ThemeStore *theme,
                               Translations *translations, SettingsService *settingsService,
                               CommandBus *commands, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_vehicle(vehicle)
    , m_theme(theme)
    , m_translations(translations)
    , m_settingsService(settingsService)
    , m_commands(commands)
{
    // Rebuild menu when settings or language change
    connect(m_settings, &SettingsStore::themeChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::languageChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::blinkerStyleChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::dualBatteryChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::hornWhenSeatboxOpenChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::batteryDisplayModeChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::routePreferenceChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::avoidCobblestoneChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::valhallaUrlChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::powerDisplayModeChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmEnabledChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmHonkChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmDurationChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showGpsChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showBluetoothChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showCloudChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showInternetChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showClockChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showTemperatureChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showCbBatteryChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showAuxBatteryChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapCheckForUpdatesChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapAutoDownloadChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapViewModeChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapNorthOrientedChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::milestoneCelebrationsChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::serviceActiveChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::otaChannelChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::otaMethodChanged, this, &MenuController::rebuildMenuTree);
    connect(m_settings, &SettingsStore::otaCheckIntervalChanged, this, &MenuController::rebuildMenuTree);
    connect(m_translations, &Translations::languageChanged, this, &MenuController::rebuildMenuTree);

    // Close menu when vehicle starts moving
    connect(m_vehicle, &VehicleStore::stateChanged, this, [this]() {
        if (m_isOpen && !m_vehicle->isParked()) {
            close();
        }
    });

    rebuildMenuTree();

    // Reset dashboard:menu-open at startup so a prior crash can't leave it stuck at "true"
    if (m_commands)
        m_commands->setMenuOpen(false);
}

MenuController::~MenuController() = default;

void MenuController::setNavigationService(NavigationService *svc)
{
    m_navigationService = svc;
    if (m_navigationService) {
        // Both signals: nav_stop's predicate reads hasRoute(), which notifies
        // on routeChanged, and the rest of the Navigation submenu turns on
        // status. Every path that clears the route happens to change status
        // too today, so statusChanged alone would work by coincidence.
        connect(m_navigationService, &NavigationService::statusChanged,
                this, &MenuController::rebuildMenuTree);
        connect(m_navigationService, &NavigationService::routeChanged,
                this, &MenuController::rebuildMenuTree);
    }
    rebuildMenuTree();
}

void MenuController::setSavedLocationsStore(SavedLocationsStore *store)
{
    m_savedLocations = store;
    if (m_savedLocations) {
        connect(m_savedLocations, &SavedLocationsStore::locationsChanged,
                this, &MenuController::rebuildMenuTree);
    }
    rebuildMenuTree();
}

void MenuController::setRecentDestinationsStore(RecentDestinationsStore *store)
{
    m_recentDestinations = store;
    if (m_recentDestinations) {
        connect(m_recentDestinations, &RecentDestinationsStore::destinationsChanged,
                this, &MenuController::rebuildMenuTree);
    }
    rebuildMenuTree();
}

void MenuController::setNavigator(Navigator *navigator)
{
    m_navigator = navigator;
}

void MenuController::setNavigationAvailabilityService(NavigationAvailabilityService *svc)
{
    m_navAvailability = svc;
    if (m_navAvailability) {
        connect(m_navAvailability, &NavigationAvailabilityService::availabilityChanged,
                this, &MenuController::rebuildMenuTree);
    }
}

void MenuController::setInternetStore(InternetStore *store)
{
    m_internet = store;
}

void MenuController::setHopOnService(HopOnService *svc)
{
    m_hopOn = svc;
    if (m_hopOn) {
        // Re-render the menu when the combo state changes (no combo <->
        // has combo flips this entry between an action and a submenu).
        connect(m_hopOn, &HopOnService::comboChanged,
                this, &MenuController::rebuildMenuTree);
    }
}

void MenuController::setMapDownloadService(MapDownloadService *svc)
{
    m_mapDownload = svc;
    if (m_mapDownload) {
        connect(m_mapDownload, &MapDownloadService::updateAvailableChanged,
                this, &MenuController::rebuildMenuTree);
        connect(m_mapDownload, &MapDownloadService::updateCheckCompleted,
                this, &MenuController::rebuildMenuTree);
    }
}

void MenuController::setFaultsService(FaultsService *svc)
{
    m_faults = svc;
    if (m_faults) {
        connect(m_faults, &FaultsService::entriesChanged,
                this, &MenuController::rebuildMenuTree);
    }
}

void MenuController::setToastService(ToastService *svc)
{
    m_toastService = svc;
}

void MenuController::setUpdateChannelService(UpdateChannelService *svc)
{
    m_updateChannel = svc;
}

MenuContext MenuController::makeContext() const
{
    MenuContext ctx;
    ctx.settings = m_settings;
    ctx.vehicle = m_vehicle;
    ctx.theme = m_theme;
    ctx.tr = m_translations;
    ctx.navigator = m_navigator;
    ctx.navigationService = m_navigationService;
    ctx.navAvailability = m_navAvailability;
    ctx.internet = m_internet;
    ctx.hopOn = m_hopOn;
    ctx.mapDownload = m_mapDownload;
    ctx.faults = m_faults;
    ctx.savedLocations = m_savedLocations;
    ctx.recentDestinations = m_recentDestinations;
    ctx.updateChannel = m_updateChannel;
    return ctx;
}

void MenuController::rebuildMenuTree()
{
    // Skip rebuilds if the menu is closed. We'll rebuild when it opens.
    if (!m_isOpen) return;

    // Skip signal-triggered rebuilds while an action is executing.
    // runGuarded() will call rebuildMenuTree() once after the action completes.
    if (m_executingAction) return;

    // Store the current path to restore it if possible
    auto savedPath = m_pathStack;
    auto savedIndex = m_selectedIndex;
    auto savedIndexStack = m_indexStack;

    m_rootNode = MenuDefinition::buildMenuTree(makeContext());

    // Restore path if possible
    m_pathStack.clear();
    m_indexStack.clear();
    MenuNode *current = m_rootNode.get();
    for (int i = 0; i < savedPath.size(); ++i) {
        bool found = false;
        for (auto *child : current->visibleChildren()) {
            if (child->id() == savedPath[i]) {
                m_pathStack.append(savedPath[i]);
                m_indexStack.append(savedIndexStack[i]);
                current = child;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    // Restore onto the row the selection was last known to be on, by id.
    // m_selectedId was recorded while the list still had its old shape, which
    // is the whole point: an entry appearing or disappearing above it shifts
    // every row below, and cycling Navigation Routing to Offline reveals Avoid
    // Cobblestone higher up the same list.
    const auto restoredChildren = current->visibleChildren();
    int selected = -1;
    if (!m_selectedId.isEmpty()) {
        for (int i = 0; i < restoredChildren.size(); ++i) {
            if (restoredChildren[i]->id() == m_selectedId) {
                selected = i;
                break;
            }
        }
    }
    // The row itself can be the one that went away, in which case its old
    // position is the closest thing to where the rider was looking.
    m_selectedIndex = selected >= 0
                    ? selected
                    : qBound(0, savedIndex, qMax(0, (int)restoredChildren.size() - 1));
    rememberSelection();

    emitMenuChanged();
}

void MenuController::rememberSelection()
{
    m_selectedId.clear();
    if (MenuNode *node = findCurrentNode()) {
        const auto children = node->visibleChildren();
        if (m_selectedIndex >= 0 && m_selectedIndex < children.size())
            m_selectedId = children[m_selectedIndex]->id();
    }
}

MenuNode *MenuController::findCurrentNode() const
{
    if (!m_rootNode) return nullptr;

    MenuNode *node = m_rootNode.get();
    for (const auto &id : m_pathStack) {
        bool found = false;
        for (auto *child : node->visibleChildren()) {
            if (child->id() == id) {
                node = child;
                found = true;
                break;
            }
        }
        if (!found) return m_rootNode.get();
    }
    return node;
}

// The level a Back lands on, for the hold hint. Empty at the root, where the
// hold leaves the menu instead of going up.
QString MenuController::parentTitle() const
{
    if (m_pathStack.isEmpty() || !m_rootNode)
        return {};
    if (m_pathStack.size() == 1)
        return m_translations->menuMainMenu();

    MenuNode *node = m_rootNode.get();
    for (int i = 0; i < m_pathStack.size() - 1; ++i) {
        MenuNode *next = nullptr;
        for (auto *child : node->visibleChildren()) {
            if (child->id() == m_pathStack[i]) {
                next = child;
                break;
            }
        }
        if (!next)
            return {};
        node = next;
    }
    return node->title();
}

// The row a right long-tap would act on: the selected row's declared primary
// child, looked up among the children it would show if entered. Returns
// nothing when the row declares none, or when the child it names is hidden by
// its own predicate.
MenuNode *MenuController::selectedPrimaryNode() const
{
    MenuNode *node = findCurrentNode();
    if (!node)
        return nullptr;

    const auto rows = node->visibleChildren();
    if (m_selectedIndex < 0 || m_selectedIndex >= rows.size())
        return nullptr;

    const QString primaryId = rows[m_selectedIndex]->primaryChildId();
    if (primaryId.isEmpty())
        return nullptr;

    for (auto *child : rows[m_selectedIndex]->visibleChildren()) {
        if (child->id() == primaryId)
            return child;
    }
    return nullptr;
}

QString MenuController::selectedPrimaryLabel() const
{
    MenuNode *primary = selectedPrimaryNode();
    return primary ? primary->title() : QString();
}

void MenuController::activatePrimary()
{
    MenuNode *primary = selectedPrimaryNode();
    if (!primary)
        return;

    runGuarded(primary->action());
}

// Actions signal rebuilds (starting a route or touching the saved list
// notifies immediately) that would destroy the tree the caller is standing
// in. The guard defers them; one rebuild runs after the action returns.
void MenuController::runGuarded(const MenuAction &action)
{
    m_executingAction = true;
    runAction(action);
    m_executingAction = false;
    rebuildMenuTree();
}

QString MenuController::currentTitle() const
{
    auto *node = findCurrentNode();
    return node ? node->headerTitle() : m_translations->menuTitle();
}

QVariantList MenuController::currentItems() const
{
    auto *node = findCurrentNode();
    if (!node) return {};

    QVariantList list;

    // No synthetic back row: the bottom bar names the hold that goes back, so
    // a row that does the same thing would cost every submenu a line.
    for (auto *child : node->visibleChildren()) {
        QVariantMap item;
        item[QStringLiteral("id")] = child->id();
        item[QStringLiteral("title")] = child->title();
        item[QStringLiteral("type")] = child->type() == MenuNodeType::Action ? QStringLiteral("action")
                                     : child->type() == MenuNodeType::Submenu ? QStringLiteral("submenu")
                                     : child->type() == MenuNodeType::CycleSetting ? QStringLiteral("cycle")
                                     : QStringLiteral("setting");
        item[QStringLiteral("currentValue")] = child->currentValue();
        item[QStringLiteral("hasChildren")] = child->hasChildren();
        if (child->type() == MenuNodeType::CycleSetting || !child->currentValueLabel().isEmpty())
            item[QStringLiteral("valueLabel")] = child->currentValueLabel();
        if (child->caution())
            item[QStringLiteral("caution")] = true;
        if (!child->leadingIcon().isEmpty())
            item[QStringLiteral("leadingIcon")] = child->leadingIcon();
        list.append(item);
    }
    return list;
}

bool MenuController::canScroll() const
{
    auto *node = findCurrentNode();
    if (!node) return false;
    return node->visibleChildren().size() > 1;
}

void MenuController::toggle()
{
    if (m_isOpen)
        close();
    else
        open();
}

void MenuController::open()
{
    openAt({}, {}, 0);
}

void MenuController::openAt(const QStringList &path, const QList<int> &indexStack, int index)
{
    qDebug() << "MenuController: open requested, vehicleState" << m_vehicle->state()
             << "isOpen" << m_isOpen << "hopOnMode" << (m_hopOn ? m_hopOn->mode() : -1)
             << "path" << path;

    if (!m_vehicle->isParked()) {
        qDebug() << "MenuController: open dropped - not parked, vehicleState" << m_vehicle->state();
        return;
    }
    if (m_isOpen) {
        qDebug() << "MenuController: open dropped - already open";
        return;
    }
    if (m_hopOn && m_hopOn->mode() != HopOnService::Idle) {
        qDebug() << "MenuController: open dropped - hop-on not idle, mode" << m_hopOn->mode();
        return;
    }

    qDebug() << "MenuController: opening menu";
    clearResume();
    m_isOpen = true;
    // rebuildMenuTree() replays the path against the tree it just built and
    // stops at the first level that no longer exists, so a stale path lands
    // on the nearest surviving ancestor rather than nowhere.
    m_pathStack = path;
    m_indexStack = indexStack;
    m_selectedIndex = index;
    m_openedAt.start();
    rebuildMenuTree();
    if (m_commands)
        m_commands->setMenuOpen(true);
    emit isOpenChanged();
}

void MenuController::closeForScreen()
{
    const QStringList path = m_pathStack;
    const QList<int> indexStack = m_indexStack;
    const int index = m_selectedIndex;
    close();
    m_resumePath = path;
    m_resumeIndexStack = indexStack;
    m_resumeIndex = index;
    m_resumeArmed = true;
}

void MenuController::resume()
{
    if (!m_resumeArmed) return;
    const QStringList path = m_resumePath;
    const QList<int> indexStack = m_resumeIndexStack;
    const int index = m_resumeIndex;
    clearResume();
    openAt(path, indexStack, index);
}

void MenuController::clearResume()
{
    m_resumeArmed = false;
    m_resumePath.clear();
    m_resumeIndexStack.clear();
    m_resumeIndex = 0;
}

void MenuController::close()
{
    clearResume();
    if (!m_isOpen) return;
    m_isOpen = false;
    m_selectedIndex = 0;
    m_pathStack.clear();
    m_indexStack.clear();
    if (m_commands)
        m_commands->setMenuOpen(false);
    emit isOpenChanged();
    emitMenuChanged();
}

void MenuController::navigateDown()
{
    if (m_openedAt.isValid() && m_openedAt.elapsed() < kOpenInputGraceMs) return;
    auto *node = findCurrentNode();
    if (!node) return;
    int totalCount = node->visibleChildren().size();
    if (totalCount <= 1) return;
    m_selectedIndex = (m_selectedIndex + 1) % totalCount;
    rememberSelection();
    emitMenuChanged();
}

void MenuController::selectItem()
{
    auto *node = findCurrentNode();
    if (!node) return;

    auto children = node->visibleChildren();
    if (m_selectedIndex < 0 || m_selectedIndex >= children.size()) return;

    auto *selected = children[m_selectedIndex];

    if (selected->type() == MenuNodeType::CycleSetting) {
        // Inline cycle: advance to next option and apply it
        runGuarded(selected->nextCycleAction());
    } else if (selected->type() == MenuNodeType::Submenu && selected->hasChildren()) {
        // Enter submenu
        m_pathStack.append(selected->id());
        m_indexStack.append(m_selectedIndex);
        m_selectedIndex = 0;
        rememberSelection();
        emitMenuChanged();
    } else {
        runGuarded(selected->action());
    }
}

void MenuController::goBack()
{
    if (m_pathStack.isEmpty()) {
        close();
    } else {
        const QString leaving = m_pathStack.takeLast();
        m_selectedIndex = m_indexStack.isEmpty() ? 0 : m_indexStack.takeLast();
        // Land back on the row that was entered, found by id. The level may
        // have gained or lost entries while the rider was inside it, and the
        // stored index would then point at whatever slid into that slot.
        if (MenuNode *node = findCurrentNode()) {
            const auto children = node->visibleChildren();
            for (int i = 0; i < children.size(); ++i) {
                if (children[i]->id() == leaving) {
                    m_selectedIndex = i;
                    break;
                }
            }
            m_selectedIndex = qBound(0, m_selectedIndex, qMax(0, (int)children.size() - 1));
        }
        rememberSelection();
        emitMenuChanged();
    }
}

void MenuController::emitMenuChanged()
{
    emit menuChanged();
}

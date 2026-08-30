#pragma once

#include <memory>

class MenuNode;
class SettingsStore;
class VehicleStore;
class ThemeStore;
class Translations;
class Navigator;
class NavigationService;
class NavigationAvailabilityService;
class InternetStore;
class HopOnService;
class MapDownloadService;
class FaultsService;
class SavedLocationsStore;
class RecentDestinationsStore;
class UpdateChannelService;

// Everything the menu tree reads. Collaborators may be null while the app is
// still wiring up; the definition treats each as optional. The definition
// never mutates through these pointers - all mutation goes through
// MenuController::runAction.
struct MenuContext {
    SettingsStore *settings = nullptr;
    VehicleStore *vehicle = nullptr;
    ThemeStore *theme = nullptr;
    Translations *tr = nullptr;
    Navigator *navigator = nullptr;
    NavigationService *navigationService = nullptr;
    NavigationAvailabilityService *navAvailability = nullptr;
    InternetStore *internet = nullptr;
    HopOnService *hopOn = nullptr;
    MapDownloadService *mapDownload = nullptr;
    FaultsService *faults = nullptr;
    SavedLocationsStore *savedLocations = nullptr;
    RecentDestinationsStore *recentDestinations = nullptr;
    UpdateChannelService *updateChannel = nullptr;
};

namespace MenuDefinition {

// Builds the whole menu tree as data: labels and structure are rendered from
// the context's current state, actions are MenuAction verbs, and visibility
// predicates capture the context (by value; it is a struct of pointers) so
// they stay live between rebuilds.
std::unique_ptr<MenuNode> buildMenuTree(const MenuContext &ctx);

// True when a route can be computed: local Valhalla answers, or the scooter
// is online with online routing configured.
bool isRoutingReady(const MenuContext &ctx);

}

#pragma once

#include <QString>
#include <QVariant>

// What a menu row does when activated, as data. The tree definition
// (MenuDefinition.cpp) only names a verb and its argument; the single
// interpreter (MenuController::runAction in MenuActions.cpp) is the one
// place that calls services, so the menu's entire outbound surface is
// greppable by verb.
struct MenuAction {
    enum class Verb {
        None,
        CloseMenu,

        // CommandBus, then close
        LockVehicle,
        ToggleHazards,
        EnableServiceMode,
        DisableServiceMode,
        ClearBluetoothBonds,

        // closeForScreen, then hand the display to a full-screen page
        ShowScreen,          // key: "address-selection" | "faults" | "about"
                             //      | "hop-on-info" | "update-mode-info"
        ShowNavigationSetup, // value: SetupMode int
        ShowSystemInfo,      // value: SystemInfoPage int

        SwitchView,          // key: "cluster" | "map"

        HopOnActivate,
        HopOnRelearn,
        HopOnDisable,

        StopNavigation,
        StartRecent,         // value: recent-destination id
        SaveRecent,
        DeleteRecent,
        SaveCurrentLocation,
        StartSaved,          // value: saved-location id
        DeleteSaved,

        SetSetting,          // key: setting id, value: new value
        ToggleSetting,       // key: setting id (reads current state live)

        CheckMapUpdates,
        CheckOsUpdates,
        SetOtaMethod,        // value: "delta" | "full"
        SwitchUpdateChannel, // value: channel name
        CaptureLogs,
    };

    Verb verb = Verb::None;
    QString key;
    QVariant value;
};

import QtQuick
import ScootUI 1.0
import "screens"
import "widgets/blinker"
import "widgets/shutdown"
import "overlays"

Window {
    id: root
    width: typeof appWidth !== "undefined" ? appWidth : 480
    height: typeof appHeight !== "undefined" ? appHeight : 480
    visible: true
    color: "black"
    title: "ScootUI"

    readonly property var allowedStates: [
        Scooter.VehicleState.Unknown,
        Scooter.VehicleState.ReadyToDrive,
        Scooter.VehicleState.Parked,
        Scooter.VehicleState.ShuttingDown,
        Scooter.VehicleState.WaitingHibernation,
        Scooter.VehicleState.WaitingHibernationAdvanced,
        Scooter.VehicleState.WaitingHibernationSeatbox,
        Scooter.VehicleState.WaitingHibernationConfirm
    ]
    readonly property int vehicleState: typeof vehicleStore !== "undefined" ? vehicleStore.state : Scooter.VehicleState.Unknown
    readonly property int currentScreen: typeof screenStore !== "undefined" ? screenStore.currentScreen : 0

    readonly property bool showMaintenance: {
        // Prolonged Redis disconnect before ever connecting
        if (typeof connectionStore !== "undefined"
            && connectionStore.prolongedDisconnect
            && !connectionStore.hasEverConnected) return true
        if (allowedStates.indexOf(vehicleState) === -1) return true
        if (vehicleState === Scooter.VehicleState.Unknown && startupGraceElapsed) return true
        return false
    }

    // Show connection info only for genuine connection failures, not for locked/transitional states
    readonly property bool maintenanceShowConnectionInfo: {
        if (typeof connectionStore !== "undefined"
            && connectionStore.prolongedDisconnect
            && !connectionStore.hasEverConnected) return true
        if (vehicleState === Scooter.VehicleState.Unknown && startupGraceElapsed) return true
        return false
    }

    property bool startupGraceElapsed: false
    property bool startupOverlaysLoaded: false
    property bool _versionOverlayRequested: false

    Timer {
        id: startupTimer
        interval: 5000
        running: true
        onTriggered: root.startupGraceElapsed = true
    }

    // Load deferred overlays (e.g. ToastOverlay) one event-loop tick after
    // the first frame so they don't delay the initial render.
    Timer {
        interval: 0
        running: true
        onTriggered: root.startupOverlaysLoaded = true
    }

    // Version overlay activation (mirrors the 3-second brake-hold logic that
    // was inside the eagerly-loaded VersionOverlay)
    Timer {
        id: versionHoldTimer
        interval: 3000
        running: typeof vehicleStore !== "undefined"
                 && vehicleStore.brakeLeft === 0 && vehicleStore.brakeRight === 0
                 && vehicleStore.state === 4
                 && typeof menuStore !== "undefined" && !menuStore.isOpen
                 && !root._versionOverlayRequested
        onTriggered: root._versionOverlayRequested = true
    }
    Connections {
        target: typeof vehicleStore !== "undefined" ? vehicleStore : null
        function onBrakeLeftChanged()  { if (vehicleStore.brakeLeft  !== 0) root._versionOverlayRequested = false }
        function onBrakeRightChanged() { if (vehicleStore.brakeRight !== 0) root._versionOverlayRequested = false }
    }

    // Cancel startup timer when vehicle state becomes known;
    // auto-close parked-only screens when riding starts
    Connections {
        target: typeof vehicleStore !== "undefined" ? vehicleStore : null
        function onStateChanged() {
            if (vehicleStore.state !== Scooter.VehicleState.Unknown) {
                startupTimer.stop()
            }
            if (vehicleStore.state === Scooter.VehicleState.ReadyToDrive
                    && typeof screenStore !== "undefined") {
                if (screenStore.currentScreen === Scooter.ScreenMode.About)
                    screenStore.closeAbout()
            }
        }
    }

    // Show permanent toast on mid-session Redis disconnect
    Connections {
        target: typeof connectionStore !== "undefined" ? connectionStore : null
        function onProlongedDisconnectChanged() {
            if (typeof connectionStore !== "undefined" && typeof toastService !== "undefined") {
                if (connectionStore.prolongedDisconnect && connectionStore.hasEverConnected) {
                    toastService.showPermanentError(
                        typeof translations !== "undefined"
                            ? translations.redisDisconnected
                            : "System connection lost",
                        "redis-disconnect"
                    )
                } else {
                    toastService.dismiss("redis-disconnect")
                }
            }
        }
        function onUsingBackupConnectionChanged() {
            if (typeof connectionStore !== "undefined" && typeof toastService !== "undefined") {
                if (connectionStore.usingBackupConnection) {
                    toastService.showPermanentError(
                        typeof translations !== "undefined"
                            ? translations.usbDisconnected
                            : "USB connection interrupted",
                        "usb-disconnect"
                    )
                } else {
                    toastService.dismiss("usb-disconnect")
                }
            }
        }
    }

    // Double-tap left brake opens menu on main screens
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: typeof menuStore !== "undefined" && !menuStore.isOpen
                 && (root.currentScreen === Scooter.ScreenMode.Cluster
                     || root.currentScreen === Scooter.ScreenMode.Map)
        function onLeftDoubleTap() { menuStore.open() }
    }

    // Wire maintenanceShowConnectionInfo into loaded MaintenanceScreen
    Connections {
        target: screenLoader
        function onLoaded() {
            if (screenLoader.item && "showConnectionInfo" in screenLoader.item) {
                screenLoader.item.showConnectionInfo = Qt.binding(function() {
                    return root.maintenanceShowConnectionInfo
                })
            }
        }
    }

    // Screen switcher
    Loader {
        id: screenLoader
        anchors.fill: parent
        sourceComponent: {
            var maint = root.showMaintenance
            var screen = root.currentScreen
            if (maint) {
                console.log("SCREEN: maintenance (showMaintenance=true, vehicleState=" + root.vehicleState + ")")
                return maintenanceComponent
            }
            var name = "unknown"
            var comp = clusterComponent
            switch (screen) {
                case Scooter.ScreenMode.Cluster:         comp = clusterComponent;     name = "cluster";     break
                case Scooter.ScreenMode.Map:             comp = mapComponent;         name = "map";         break
                case Scooter.ScreenMode.Debug:           comp = debugComponent;       name = "debug";       break
                case Scooter.ScreenMode.About:           comp = aboutComponent;       name = "about";       break
                case Scooter.ScreenMode.Maintenance:     comp = maintenanceComponent; name = "maintenance"; break
                case Scooter.ScreenMode.Ota:             comp = otaBgComponent;       name = "otaBg";       break
                case Scooter.ScreenMode.AddressSelection:comp = addressComponent;     name = "address";     break
                case Scooter.ScreenMode.NavigationSetup: comp = navSetupComponent;    name = "navSetup";    break
                case Scooter.ScreenMode.Destination:     comp = destinationComponent; name = "destination"; break
                default:                                    comp = clusterComponent;     name = "cluster(default)"; break
            }
            console.log("SCREEN: " + name + " (screen=" + screen + ")")
            return comp
        }
    }

    Component { id: clusterComponent; ClusterScreen {} }
    Component { id: mapComponent; MapScreen {} }
    Component { id: maintenanceComponent; MaintenanceScreen {} }
    Component { id: aboutComponent; AboutScreen {} }
    Component { id: debugComponent; DebugScreen {} }
    Component { id: otaBgComponent; OtaBackgroundScreen {} }
    Component { id: addressComponent; AddressSelectionScreen {} }
    Component { id: navSetupComponent; NavigationSetupScreen {} }
    Component { id: destinationComponent; DestinationScreen {} }

    // Overlays (bottom to top stacking order)
    //
    // Most overlays are wrapped in Loader to defer QML compilation and
    // instantiation until they are actually needed.  This shaves ~0.5-1.5 s
    // off the initial engine.load() on the i.MX6.

    // Toast notifications — loaded early (after a single event-loop tick) so
    // boot-time toasts are not lost, but still deferred from the first frame.
    Loader {
        anchors.fill: parent; z: 900
        active: root.startupOverlaysLoaded
        sourceComponent: Component { ToastOverlay { anchors.fill: parent } }
    }

    // Blinker overlay — only when blinker is active
    Loader {
        anchors.fill: parent; z: 100
        active: typeof vehicleStore !== "undefined" && vehicleStore.blinkerState !== 0
        sourceComponent: Component {
            BlinkerOverlay {
                anchors.fill: parent
                topInset: 40
                bottomInset: screenLoader.item && typeof screenLoader.item.bottomBarHeight === "number"
                             ? screenLoader.item.bottomBarHeight : 48
            }
        }
    }

    // Menu overlay — only when menu is open
    Loader {
        anchors.fill: parent; z: 200
        active: typeof menuStore !== "undefined" && menuStore.isOpen
        sourceComponent: Component {
            MenuOverlay {
                anchors.fill: parent
                blurSource: screenLoader
            }
        }
    }

    // Shortcut menu overlay
    Loader {
        anchors.fill: parent; z: 300
        active: typeof shortcutMenuStore !== "undefined" && shortcutMenuStore.visible
        sourceComponent: Component {
            ShortcutMenuOverlay {
                anchors.fill: parent
                blurSource: screenLoader
            }
        }
    }

    // Debug overlay — only when debug mode is "overlay"
    Loader {
        anchors.fill: parent; z: 50
        active: typeof dashboardStore !== "undefined" && dashboardStore.debugMode === "overlay"
        sourceComponent: Component { DebugOverlay { anchors.fill: parent } }
    }

    // Auto-lock countdown — only when parked and countdown is active
    Loader {
        anchors.fill: parent; z: 950
        active: typeof vehicleStore !== "undefined" && vehicleStore.state === 4
                && typeof autoStandbyStore !== "undefined"
                && autoStandbyStore.remainingSeconds > 0
                && autoStandbyStore.remainingSeconds <= 60
        sourceComponent: Component { AutoLockCountdownOverlay { anchors.fill: parent } }
    }

    // Hop-on learn overlay — only during learning mode
    Loader {
        anchors.fill: parent; z: 970
        active: typeof hopOnStore !== "undefined" && hopOnStore.mode === 1
        sourceComponent: Component { HopOnLearnOverlay { anchors.fill: parent } }
    }

    // Hop-on lock overlay — only when locked
    Loader {
        anchors.fill: parent; z: 980
        active: typeof hopOnStore !== "undefined" && hopOnStore.mode === 2
        sourceComponent: Component { HopOnLockOverlay { anchors.fill: parent } }
    }

    // Shutdown overlay — only during shutdown sequence
    Loader {
        anchors.fill: parent; z: 1000
        active: typeof shutdownStore !== "undefined"
                && (shutdownStore.isShuttingDown || shutdownStore.showBlackout)
        sourceComponent: Component { ShutdownOverlay { anchors.fill: parent } }
    }

    // UMS overlay — only during USB mass storage operations
    Loader {
        anchors.fill: parent; z: 1100
        active: typeof usbStore !== "undefined" && usbStore.status !== "idle" && usbStore.status !== ""
        sourceComponent: Component { UmsOverlay { anchors.fill: parent } }
    }

    // Version overlay — only when explicitly triggered (both brakes held 3s)
    Loader {
        anchors.fill: parent; z: 1150
        active: root._versionOverlayRequested
        sourceComponent: Component { VersionOverlay { anchors.fill: parent } }
    }

    // Manual hibernation overlay
    Loader {
        anchors.fill: parent; z: 1200
        active: typeof vehicleStore !== "undefined"
                && (vehicleStore.state === 7 || vehicleStore.state === 8
                    || vehicleStore.state === 13 || vehicleStore.state === 14
                    || vehicleStore.state === 15 || vehicleStore.state === 16)
        sourceComponent: Component { ManualHibernationOverlay { anchors.fill: parent } }
    }

    // Bluetooth PIN code overlay
    Loader {
        anchors.fill: parent; z: 1300
        active: typeof bluetoothStore !== "undefined" && bluetoothStore.pinCode !== ""
        sourceComponent: Component { BluetoothPinCodeOverlay { anchors.fill: parent } }
    }
}

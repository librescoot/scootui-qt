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

    // In simulator mode, position left of center so the simulator panel fits beside it
    x: simulatorMode ? Screen.width / 2 - (width + 480 + 20) / 2 : Screen.desktopAvailableWidth / 2 - width / 2
    y: Screen.height / 2 - height / 2

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
    readonly property int vehicleState: VehicleStore.state
    readonly property int currentScreen: ScreenStore.currentScreen

    readonly property bool showMaintenance: {
        // Prolonged Redis disconnect before ever connecting
        if (ConnectionStore.prolongedDisconnect
            && !ConnectionStore.hasEverConnected) return true
        if (allowedStates.indexOf(vehicleState) === -1) return true
        if (vehicleState === Scooter.VehicleState.Unknown && root.startupGraceElapsed) return true
        return false
    }

    // Show connection info only for genuine connection failures, not for locked/transitional states
    readonly property bool maintenanceShowConnectionInfo: {
        if (ConnectionStore.prolongedDisconnect
            && !ConnectionStore.hasEverConnected) return true
        if (vehicleState === Scooter.VehicleState.Unknown && root.startupGraceElapsed) return true
        return false
    }

    property bool startupGraceElapsed: false

    Timer {
        id: startupTimer
        interval: 5000
        running: true
        onTriggered: root.startupGraceElapsed = true
    }

    // Cancel startup timer when vehicle state becomes known;
    // auto-close parked-only screens when riding starts
    Connections {
        target: VehicleStore
        function onStateChanged() {
            if (VehicleStore.state !== Scooter.VehicleState.Unknown) {
                startupTimer.stop()
            }
            if (VehicleStore.state === Scooter.VehicleState.ReadyToDrive
                    && true) {
                if (ScreenStore.currentScreen === Scooter.ScreenMode.About)
                    ScreenStore.closeAbout()
            }
        }
    }

    // Show permanent toast on mid-session Redis disconnect
    Connections {
        target: ConnectionStore
        function onProlongedDisconnectChanged() {
            if (true) {
                if (ConnectionStore.prolongedDisconnect && ConnectionStore.hasEverConnected) {
                    ToastService.showPermanentError(
                        Translations.redisDisconnected,
                        "redis-disconnect"
                    )
                } else {
                    ToastService.dismiss("redis-disconnect")
                }
            }
        }
        function onUsingBackupConnectionChanged() {
            if (true) {
                if (ConnectionStore.usingBackupConnection) {
                    ToastService.showPermanentError(
                        Translations.usbDisconnected,
                        "usb-disconnect"
                    )
                } else {
                    ToastService.dismiss("usb-disconnect")
                }
            }
        }
    }

    // Double-tap left brake opens menu on main screens
    Connections {
        target: InputHandler
        enabled: !MenuStore.isOpen
                 && (root.currentScreen === Scooter.ScreenMode.Cluster
                     || root.currentScreen === Scooter.ScreenMode.Map)
        function onLeftDoubleTap() { MenuStore.open() }
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

    // Overlays (bottom to top stacking order).
    //
    // The two that can render in the first few seconds of boot (blinker
    // while idle-ready, toast if redis is slow to connect) are
    // instantiated eagerly. Everything else is wrapped in an async
    // Loader so engine.load() doesn't pay their instantiation cost on
    // the main thread — they load across the frames after first paint.
    // Each overlay's own `visible` binding still governs what shows
    // when; the Loader just controls when it exists.

    BlinkerOverlay {
        anchors.fill: parent
        z: 100
        topInset: 40
        bottomInset: screenLoader.item && typeof screenLoader.item.bottomBarHeight === "number"
                     ? screenLoader.item.bottomBarHeight : 48
    }

    ToastOverlay {
        anchors.fill: parent
        z: 900
    }

    Loader {
        anchors.fill: parent
        z: 50
        asynchronous: true
        sourceComponent: Component { DebugOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 200
        asynchronous: true
        sourceComponent: Component {
            MenuOverlay {
                anchors.fill: parent
                blurSource: screenLoader
            }
        }
    }

    Loader {
        anchors.fill: parent
        z: 300
        asynchronous: true
        sourceComponent: Component {
            ShortcutMenuOverlay {
                anchors.fill: parent
                blurSource: screenLoader
            }
        }
    }

    Loader {
        anchors.fill: parent
        z: 920
        asynchronous: true
        sourceComponent: Component { OdometerMilestoneOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 950
        asynchronous: true
        sourceComponent: Component { AutoLockCountdownOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 970
        asynchronous: true
        sourceComponent: Component { HopOnLearnOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 980
        asynchronous: true
        sourceComponent: Component { HopOnLockOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 1000
        asynchronous: true
        sourceComponent: Component { ShutdownOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 1100
        asynchronous: true
        sourceComponent: Component { UmsOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 1150
        asynchronous: true
        sourceComponent: Component { VersionOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 1200
        asynchronous: true
        sourceComponent: Component { ManualHibernationOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 1300
        asynchronous: true
        sourceComponent: Component { BluetoothPinCodeOverlay { anchors.fill: parent } }
    }
}

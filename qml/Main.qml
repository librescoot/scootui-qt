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

    // In desktop/simulator mode, position left of centre so the simulator panel
    // sits beside it. The 900 and 64 mirror SimulatorWindow's width and uiGap.
    x: typeof simulator !== "undefined" ? Screen.width / 2 - (width + 900 + 64) / 2 : Screen.desktopAvailableWidth / 2 - width / 2
    y: Screen.height / 2 - height / 2

    // Simulator-only debugging keys. The vehicle has no keyboard, and these are
    // bound to the simulator service which does not exist outside sim mode.
    Shortcut {
        enabled: typeof simulator !== "undefined" && simulator !== null
        sequence: "+"
        onActivated: simulator.setAutoDriveTimeScale(simulator.autoDriveTimeScale() * 2)
    }
    Shortcut {
        enabled: typeof simulator !== "undefined" && simulator !== null
        sequence: "-"
        onActivated: simulator.setAutoDriveTimeScale(simulator.autoDriveTimeScale() / 2)
    }
    Shortcut {
        enabled: typeof simulator !== "undefined" && simulator !== null
        sequence: "0"
        onActivated: simulator.setAutoDriveTimeScale(1)
    }
    // Jump ahead 20 simulated seconds, enough to clear the maneuver in front.
    Shortcut {
        enabled: typeof simulator !== "undefined" && simulator !== null
        sequence: "f"
        onActivated: simulator.autoDriveSkip(20)
    }

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

    Timer {
        id: startupTimer
        interval: 5000
        running: true
        onTriggered: root.startupGraceElapsed = true
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
                else if (screenStore.currentScreen === Scooter.ScreenMode.Faults)
                    screenStore.closeFaults()
                else if (screenStore.currentScreen === Scooter.ScreenMode.SystemInfo)
                    screenStore.closeSystemInfo()
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
        id: doubleTapMenuOpener
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: typeof menuStore !== "undefined" && !menuStore.isOpen
                 && (root.currentScreen === Scooter.ScreenMode.Cluster
                     || root.currentScreen === Scooter.ScreenMode.Map
                     || root.currentScreen === Scooter.ScreenMode.Debug)
        function onLeftDoubleTap() {
            console.log("MENU: onLeftDoubleTap (currentScreen=" + root.currentScreen
                        + ", isOpen=" + menuStore.isOpen
                        + ", showMaintenance=" + root.showMaintenance + ")")
            menuStore.open()
        }
    }

    // Trace double-taps that miss the opener (Connections disabled because of
    // screen or isOpen). This fires whenever the opener's gate is false.
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: !doubleTapMenuOpener.enabled
        function onLeftDoubleTap() {
            console.log("MENU: leftDoubleTap dropped by QML gate (currentScreen=" + root.currentScreen
                        + ", isOpen=" + (typeof menuStore !== "undefined" ? menuStore.isOpen : "?")
                        + ", showMaintenance=" + root.showMaintenance + ")")
        }
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
            // MotionDebug bypasses the maintenance gate so it can be triggered
            // from a stand-by scooter (dev-only diagnostic, doesn't depend on
            // a running vehicle state).
            if (maint && screen !== Scooter.ScreenMode.MotionDebug) {
                console.log("SCREEN: maintenance (showMaintenance=true, vehicleState=" + root.vehicleState + ")")
                return maintenanceComponent
            }
            var name = "unknown"
            var comp = clusterComponent
            switch (screen) {
                case Scooter.ScreenMode.Cluster:         comp = clusterComponent;     name = "cluster";     break
                case Scooter.ScreenMode.Map:             comp = mapComponent;         name = "map";         break
                case Scooter.ScreenMode.Debug:           comp = debugComponent;       name = "debug";       break
                case Scooter.ScreenMode.MotionDebug:        comp = motionDebugComponent;    name = "motion-debug";   break
                case Scooter.ScreenMode.About:           comp = aboutComponent;       name = "about";       break
                case Scooter.ScreenMode.Maintenance:     comp = maintenanceComponent; name = "maintenance"; break
                case Scooter.ScreenMode.AddressSelection:comp = addressComponent;     name = "address";     break
                case Scooter.ScreenMode.NavigationSetup: comp = navSetupComponent;    name = "navSetup";    break
                case Scooter.ScreenMode.Faults:          comp = faultsComponent;      name = "faults";      break
                case Scooter.ScreenMode.SystemInfo:      comp = systemInfoComponent;  name = "systemInfo";  break
                case Scooter.ScreenMode.UpdateModeInfo:  comp = umsInfoComponent;     name = "umsInfo";     break
                case Scooter.ScreenMode.UpdateChannel:   comp = updateChannelComponent; name = "updateChannel"; break
                case Scooter.ScreenMode.HopOnInfo:       comp = hopOnInfoComponent;   name = "hopOnInfo";   break
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
    Component { id: motionDebugComponent; MotionDebugScreen {} }
    Component { id: addressComponent; AddressSelectionScreen {} }
    Component { id: navSetupComponent; NavigationSetupScreen {} }
    Component { id: faultsComponent; FaultsScreen {} }
    Component { id: systemInfoComponent; SystemInfoScreen {} }
    Component { id: umsInfoComponent; UpdateModeInfoScreen {} }
    Component { id: updateChannelComponent; UpdateChannelScreen {} }
    Component { id: hopOnInfoComponent; HopOnInfoScreen {} }

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
        screenAllowed: root.currentScreen === Scooter.ScreenMode.Cluster
                       || root.currentScreen === Scooter.ScreenMode.Map
    }

    ToastOverlay {
        anchors.fill: parent
        z: 900
    }

    // Gated on the same condition as the overlay's own `visible`, because
    // merely existing costs real work here: it binds to the fastest-moving
    // fields there are (brightness, backlight, rpm, motor current, throttle)
    // and a binding that changes a Text marks it dirty and buys a frame even
    // while invisible. On a dark maintenance screen that was 7.6 fps of
    // rendering for an overlay nobody had asked for.
    Loader {
        anchors.fill: parent
        z: 50
        asynchronous: true
        active: typeof dashboardStore !== "undefined" && dashboardStore.debugMode === "overlay"
        sourceComponent: Component { DebugOverlay { anchors.fill: parent } }
    }

    Loader {
        anchors.fill: parent
        z: 915
        asynchronous: true
        sourceComponent: Component { ServiceModeOverlay { anchors.fill: parent } }
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
        z: 925
        asynchronous: true
        sourceComponent: Component { MilestoneCelebrationOverlay { anchors.fill: parent } }
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

import QtQuick
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

    // Allowed vehicle states for normal UI
    // Unknown=0, Parked=4, ReadyToDrive=2, ShuttingDown=6,
    // WaitingHibernation=13, WaitingHibernationAdvanced=14,
    // WaitingHibernationSeatbox=15, WaitingHibernationConfirm=16
    readonly property var allowedStates: [0, 2, 4, 6, 13, 14, 15, 16]
    readonly property int vehicleState: typeof vehicleStore !== "undefined" ? vehicleStore.state : 0
    readonly property int currentScreen: typeof screenStore !== "undefined" ? screenStore.currentScreen : 0

    readonly property bool showMaintenance: {
        // Prolonged Redis disconnect before ever connecting
        if (typeof connectionStore !== "undefined"
            && connectionStore.prolongedDisconnect
            && !connectionStore.hasEverConnected) return true
        if (allowedStates.indexOf(vehicleState) === -1) return true
        if (vehicleState === 0 && startupGraceElapsed) return true
        return false
    }

    // Show connection info only for genuine connection failures, not for locked/transitional states
    readonly property bool maintenanceShowConnectionInfo: {
        if (typeof connectionStore !== "undefined"
            && connectionStore.prolongedDisconnect
            && !connectionStore.hasEverConnected) return true
        if (vehicleState === 0 && startupGraceElapsed) return true
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
            if (vehicleStore.state !== 0) {
                startupTimer.stop()
            }
            // ReadyToDrive=2: close About screen if open
            if (vehicleStore.state === 2 && typeof screenStore !== "undefined") {
                if (screenStore.currentScreen === 4) screenStore.closeAbout()
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
                case 0: comp = clusterComponent; name = "cluster"; break
                case 1: comp = mapComponent; name = "map"; break
                case 3: comp = debugComponent; name = "debug"; break
                case 4: comp = aboutComponent; name = "about"; break
                case 5: comp = maintenanceComponent; name = "maintenance"; break
                case 6: comp = otaBgComponent; name = "otaBg"; break
                case 7: comp = addressComponent; name = "address"; break
                case 9: comp = navSetupComponent; name = "navSetup"; break
                case 10: comp = destinationComponent; name = "destination"; break
                default: comp = clusterComponent; name = "cluster(default)"; break
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

    // Debug overlay (floating, lowest z)
    DebugOverlay {
        anchors.fill: parent
        z: 50
    }

    BlinkerOverlay {
        anchors.fill: parent
        z: 100
        topInset: 40
        bottomInset: screenLoader.item && typeof screenLoader.item.bottomBarHeight === "number"
                     ? screenLoader.item.bottomBarHeight : 48
    }

    MenuOverlay {
        anchors.fill: parent
        z: 200
        blurSource: screenLoader
    }

    ShortcutMenuOverlay {
        anchors.fill: parent
        z: 300
        blurSource: screenLoader
    }

    // Toast notifications
    ToastOverlay {
        anchors.fill: parent
        z: 900
    }

    AutoLockCountdownOverlay {
        anchors.fill: parent
        z: 950
    }

    ShutdownOverlay {
        anchors.fill: parent
        z: 1000
    }

    UmsOverlay {
        anchors.fill: parent
        z: 1100
    }

    VersionOverlay {
        anchors.fill: parent
        z: 1150
    }

    ManualHibernationOverlay {
        anchors.fill: parent
        z: 1200
    }

    BluetoothPinCodeOverlay {
        anchors.fill: parent
        z: 1300
    }
}

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

    property bool startupGraceElapsed: false

    Timer {
        id: startupTimer
        interval: 5000
        running: true
        onTriggered: root.startupGraceElapsed = true
    }

    // Cancel startup timer when vehicle state becomes known
    Connections {
        target: typeof vehicleStore !== "undefined" ? vehicleStore : null
        function onStateChanged() {
            if (vehicleStore.state !== 0) {
                startupTimer.stop()
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
    }

    // Screen switcher
    Loader {
        id: screenLoader
        anchors.fill: parent
        sourceComponent: {
            if (root.showMaintenance) return maintenanceComponent
            switch (root.currentScreen) {
                case 0: return clusterComponent      // Cluster
                case 1: return mapComponent           // Map
                case 3: return debugComponent         // Debug
                case 4: return aboutComponent         // About
                case 5: return maintenanceComponent   // Maintenance
                case 6: return otaBgComponent         // OTA Background
                case 7: return addressComponent       // Address Selection
                case 9: return navSetupComponent      // Navigation Setup
                case 10: return destinationComponent  // Destination
                default: return clusterComponent
            }
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

    ShutdownOverlay {
        anchors.fill: parent
        z: 1000
    }

    UmsOverlay {
        anchors.fill: parent
        z: 1100
    }

    ManualHibernationOverlay {
        anchors.fill: parent
        z: 1200
    }

    BluetoothPinCodeOverlay {
        anchors.fill: parent
        z: 1300
    }

    VersionOverlay {
        anchors.fill: parent
        z: 1400
    }
}

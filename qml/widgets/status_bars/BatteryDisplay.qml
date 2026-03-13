import QtQuick
import QtQuick.Effects

Row {
    id: batteryDisplay
    spacing: 8
    height: 24

    // Theme-aware icon color (matches Flutter's ColorFilter.mode srcIn)
    readonly property color iconColor: typeof themeStore !== "undefined" && !themeStore.isDark
                                        ? "#000000" : "#FFFFFF"
    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true

    // --- Enum int values ---
    // BatteryState: Unknown=0, Asleep=1, Idle=2, Active=3
    readonly property int bsAsleep: 1
    readonly property int bsIdle: 2
    readonly property int bsActive: 3

    // SeatboxLock: Open=0, Closed=1
    readonly property int slClosed: 1

    // ChargeStatus: Charging=0, NotCharging=1
    readonly property int csNotCharging: 1

    // AuxChargeStatus: NotCharging=0
    readonly property int acsNotCharging: 0

    // --- Battery 0 ---
    readonly property int charge0: typeof battery0Store !== "undefined" ? battery0Store.charge : 0
    readonly property bool present0: typeof battery0Store !== "undefined" ? battery0Store.present : false
    readonly property int soh0: typeof battery0Store !== "undefined" ? battery0Store.stateOfHealth : 100
    readonly property int battState0: typeof battery0Store !== "undefined" ? battery0Store.batteryState : 0
    readonly property var faults0: typeof battery0Store !== "undefined" ? battery0Store.faults : []
    readonly property bool hasFault0: present0 && faults0.length > 0

    // --- Battery 1 ---
    readonly property int charge1: typeof battery1Store !== "undefined" ? battery1Store.charge : 0
    readonly property bool present1: typeof battery1Store !== "undefined" ? battery1Store.present : false
    readonly property int soh1: typeof battery1Store !== "undefined" ? battery1Store.stateOfHealth : 100
    readonly property int battState1: typeof battery1Store !== "undefined" ? battery1Store.batteryState : 0
    readonly property var faults1: typeof battery1Store !== "undefined" ? battery1Store.faults : []
    readonly property bool hasFault1: present1 && faults1.length > 0
    readonly property bool showDual: present1

    // --- Battery display mode ---
    readonly property bool showAsRange: typeof settingsStore !== "undefined"
                                         && settingsStore.batteryDisplayMode === "range"

    function rangeText(charge, soh) {
        var rangeKm = 45.0 * (soh / 100.0) * (charge / 100.0)
        if (rangeKm >= 10)
            return Math.floor(rangeKm) + " km"
        return rangeKm.toFixed(1) + " km"
    }

    // Fill color: red/orange only for battery 0, normal for battery 1
    function fillColor(charge, isBattery0) {
        if (isBattery0) {
            if (charge <= 10) return "#FF0000"
            if (charge <= 20) return "#FF7900"
        }
        return themeStore.textColor
    }

    // --- Seatbox ---
    readonly property bool seatboxOpen: typeof vehicleStore !== "undefined"
                                         ? vehicleStore.seatboxLock !== slClosed : false

    // --- Turtle mode ---
    readonly property bool showTurtle: present0 && charge0 <= 20

    // --- Battery warning conditions ---
    // CB warning: charge < 95%, not charging, main present & active, seatbox closed
    readonly property bool cbWarningCondition: {
        if (typeof cbBatteryStore === "undefined" || typeof vehicleStore === "undefined") return false
        return cbBatteryStore.charge < 95
            && cbBatteryStore.chargeStatus === csNotCharging
            && present0 && charge0 > 0 && battState0 === bsActive
            && vehicleStore.seatboxLock === slClosed
    }
    // AUX low charge: charge ≤ 25%, not charging, main present & active, seatbox closed
    readonly property bool auxLowChargeCondition: {
        if (typeof auxBatteryStore === "undefined" || typeof vehicleStore === "undefined") return false
        return auxBatteryStore.charge <= 25
            && auxBatteryStore.chargeStatus === acsNotCharging
            && present0 && charge0 > 0 && battState0 === bsActive
            && vehicleStore.seatboxLock === slClosed
    }
    // AUX low voltage: voltage < 11500mV, not charging, main present, seatbox closed
    readonly property bool auxLowVoltageCondition: {
        if (typeof auxBatteryStore === "undefined" || typeof vehicleStore === "undefined") return false
        return auxBatteryStore.voltage < 11500
            && auxBatteryStore.chargeStatus === acsNotCharging
            && present0
            && vehicleStore.seatboxLock === slClosed
    }
    // AUX critical voltage: voltage < 11000mV, main present, seatbox closed
    readonly property bool auxCriticalCondition: {
        if (typeof auxBatteryStore === "undefined" || typeof vehicleStore === "undefined") return false
        return auxBatteryStore.voltage < 11000
            && present0
            && vehicleStore.seatboxLock === slClosed
    }

    // --- 3-second debounce for warning indicators (matching Flutter) ---
    property bool showCbWarning: false
    property bool showAuxWarning: false

    property bool _cbDebounceActive: false
    property bool _auxDebounceActive: false

    readonly property bool _anyAuxCondition: auxLowChargeCondition || auxLowVoltageCondition || auxCriticalCondition

    onCbWarningConditionChanged: {
        if (cbWarningCondition) {
            if (!_cbDebounceActive) {
                _cbDebounceActive = true
                cbDebounceTimer.restart()
            }
        } else {
            _cbDebounceActive = false
            cbDebounceTimer.stop()
            showCbWarning = false
        }
    }

    on_AnyAuxConditionChanged: {
        if (_anyAuxCondition) {
            if (!_auxDebounceActive) {
                _auxDebounceActive = true
                auxDebounceTimer.restart()
            }
        } else {
            _auxDebounceActive = false
            auxDebounceTimer.stop()
            showAuxWarning = false
        }
    }

    Timer {
        id: cbDebounceTimer
        interval: 3000
        onTriggered: {
            if (batteryDisplay.cbWarningCondition)
                batteryDisplay.showCbWarning = true
        }
    }

    Timer {
        id: auxDebounceTimer
        interval: 3000
        onTriggered: {
            if (batteryDisplay._anyAuxCondition)
                batteryDisplay.showAuxWarning = true
        }
    }

    // --- Charge bar dimensions (scaled from 144x144 to 24x24) ---
    readonly property real chargeX: 23.0 * (24.0 / 144.0)   // ~3.83
    readonly property real chargeY: 41.0 * (24.0 / 144.0)    // ~6.83
    readonly property real chargeH: 83.0 * (24.0 / 144.0)    // ~13.83
    readonly property real chargeMaxW: 98.0 * (24.0 / 144.0) // ~16.33

    // =====================================================================
    // Battery 0 icon
    // =====================================================================
    Item {
        width: 24; height: 24

        // Base battery icon (tinted for theme)
        Image {
            id: b0base
            anchors.fill: parent
            source: {
                if (!present0) return "qrc:/ScootUI/assets/icons/librescoot-main-battery-absent.svg"
                if (battState0 !== bsAsleep && battState0 !== bsIdle && charge0 <= 10)
                    return "qrc:/ScootUI/assets/icons/librescoot-main-battery-empty.svg"
                return "qrc:/ScootUI/assets/icons/librescoot-main-battery-blank.svg"
            }
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            source: b0base
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }

        // Charge bar (shown for blank icon states)
        Rectangle {
            visible: present0 && charge0 > 10
                     && (battState0 === bsActive || battState0 === bsAsleep || battState0 === bsIdle || charge0 > 10)
            x: chargeX; y: chargeY
            height: chargeH
            width: chargeMaxW * (charge0 / 100)
            color: fillColor(charge0, true)
        }

        // Asleep mask (renders in background color to "cut out" areas)
        Image {
            id: b0asleepMask
            anchors.fill: parent
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-main-battery-asleep-mask.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        MultiEffect {
            source: b0asleepMask
            anchors.fill: parent
            visible: present0 && battState0 === bsAsleep
            colorization: 1.0
            colorizationColor: batteryDisplay.isDark ? "#000000" : "#FFFFFF"
        }

        // Asleep overlay
        Image {
            id: b0asleepOverlay
            anchors.fill: parent
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-main-battery-asleep-overlay.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        MultiEffect {
            source: b0asleepOverlay
            anchors.fill: parent
            visible: present0 && battState0 === bsAsleep
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }

        // Idle overlay
        Image {
            id: b0idleOverlay
            anchors.fill: parent
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-overlay-idle.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        MultiEffect {
            source: b0idleOverlay
            anchors.fill: parent
            visible: present0 && battState0 === bsIdle
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }

        // Fault overlay (error X)
        Image {
            id: b0faultOverlay
            anchors.fill: parent
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        MultiEffect {
            source: b0faultOverlay
            anchors.fill: parent
            visible: hasFault0
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
    }

    // Battery 0 charge/range text
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: present0 ? (showAsRange ? rangeText(charge0, soh0) : charge0 + "%") : ""
        font.pixelSize: showAsRange ? 14 : 16
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        color: fillColor(charge0, true)
    }

    // =====================================================================
    // Battery 1 icon (dual mode)
    // =====================================================================
    Item {
        width: 24; height: 24
        visible: batteryDisplay.showDual

        Image {
            id: b1base
            anchors.fill: parent
            source: {
                if (!present1) return "qrc:/ScootUI/assets/icons/librescoot-main-battery-absent.svg"
                if (battState1 !== bsAsleep && battState1 !== bsIdle && charge1 <= 10)
                    return "qrc:/ScootUI/assets/icons/librescoot-main-battery-empty.svg"
                return "qrc:/ScootUI/assets/icons/librescoot-main-battery-blank.svg"
            }
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            source: b1base
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }

        // Charge bar
        Rectangle {
            visible: present1 && charge1 > 10
                     && (battState1 === bsActive || battState1 === bsAsleep || battState1 === bsIdle || charge1 > 10)
            x: chargeX; y: chargeY
            height: chargeH
            width: chargeMaxW * (charge1 / 100)
            color: fillColor(charge1, false)
        }

        // Asleep mask
        Image {
            id: b1asleepMask
            anchors.fill: parent
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-main-battery-asleep-mask.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        MultiEffect {
            source: b1asleepMask
            anchors.fill: parent
            visible: present1 && battState1 === bsAsleep
            colorization: 1.0
            colorizationColor: batteryDisplay.isDark ? "#000000" : "#FFFFFF"
        }

        // Asleep overlay
        Image {
            id: b1asleepOverlay
            anchors.fill: parent
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-main-battery-asleep-overlay.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        MultiEffect {
            source: b1asleepOverlay
            anchors.fill: parent
            visible: present1 && battState1 === bsAsleep
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }

        // Idle overlay
        Image {
            id: b1idleOverlay
            anchors.fill: parent
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-overlay-idle.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        MultiEffect {
            source: b1idleOverlay
            anchors.fill: parent
            visible: present1 && battState1 === bsIdle
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }

        // Fault overlay
        Image {
            id: b1faultOverlay
            anchors.fill: parent
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        MultiEffect {
            source: b1faultOverlay
            anchors.fill: parent
            visible: hasFault1
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
    }

    // Battery 1 charge/range text
    Text {
        anchors.verticalCenter: parent.verticalCenter
        visible: batteryDisplay.showDual
        text: present1 ? (showAsRange ? rangeText(charge1, soh1) : charge1 + "%") : ""
        font.pixelSize: showAsRange ? 14 : 16
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        color: fillColor(charge1, false)
    }

    // =====================================================================
    // Seatbox open indicator
    // =====================================================================
    Item {
        visible: batteryDisplay.seatboxOpen
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: seatboxIcon
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-seatbox-open.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            source: seatboxIcon
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
    }

    // =====================================================================
    // Battery warning indicators (CB and AUX with error overlay)
    // =====================================================================
    Item {
        visible: showCbWarning
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: cbIcon
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-cb-battery-blank.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            source: cbIcon
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
        Image {
            id: cbErrorOverlay
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            source: cbErrorOverlay
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
    }

    Item {
        visible: showAuxWarning
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: auxIcon
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-aux-battery-blank.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            source: auxIcon
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
        Image {
            id: auxErrorOverlay
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            source: auxErrorOverlay
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
    }

    // =====================================================================
    // Turtle mode icon (shown when battery ≤ 20%)
    // =====================================================================
    Item {
        visible: batteryDisplay.showTurtle
        width: 20; height: 20
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: turtleIcon
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-turtle-mode.svg"
            sourceSize: Qt.size(20, 20)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            source: turtleIcon
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
    }
}

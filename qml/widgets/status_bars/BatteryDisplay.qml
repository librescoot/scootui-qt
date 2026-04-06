import QtQuick
import QtQuick.Effects
import ScootUI

Row {
    id: batteryDisplay
    spacing: 3
    height: 24

    // Theme-aware icon color (matches Flutter's ColorFilter.mode srcIn)
    readonly property color iconColor: !ThemeStore.isDark
                                        ? "#000000" : "#FFFFFF"
    readonly property bool isDark: ThemeStore.isDark

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
    readonly property int charge0: Battery0Store.charge
    readonly property bool present0: Battery0Store.present
    readonly property int soh0: Battery0Store.stateOfHealth
    readonly property int battState0: Battery0Store.batteryState
    readonly property var faults0: Battery0Store.faults
    readonly property bool hasFault0: present0 && faults0.length > 0

    // --- Battery 1 ---
    readonly property int charge1: Battery1Store.charge
    readonly property bool present1: Battery1Store.present
    readonly property int soh1: Battery1Store.stateOfHealth
    readonly property int battState1: Battery1Store.batteryState
    readonly property var faults1: Battery1Store.faults
    readonly property bool hasFault1: present1 && faults1.length > 0
    readonly property bool showDual: present1

    // --- Battery display mode ---
    readonly property bool showAsRange: SettingsStore.batteryDisplayMode === "range"

    function rangeText(charge, soh) {
        var rangeKm = 45.0 * (soh / 100.0) * (charge / 100.0)
        if (rangeKm >= 10)
            return Math.floor(rangeKm) + " km"
        return rangeKm.toFixed(1) + " km"
    }

    function chargeLabelColor(charge, isBattery0) {
        if (isBattery0) {
            if (charge <= 10) return "#FF0000"
            if (charge <= 20) return "#FF7900"
        }
        return ThemeStore.textColor
    }

    // --- Seatbox ---
    readonly property bool seatboxOpen: VehicleStore.seatboxLock !== slClosed

    // --- Turtle mode ---
    readonly property bool showTurtle: present0 && charge0 <= 20

    // --- Battery warning conditions ---
    // CB warning: charge < 95%, not charging, main present & active, seatbox closed
    readonly property bool cbWarningCondition: {
        // types always defined
        return CbBatteryStore.charge < 95
            && CbBatteryStore.chargeStatus === csNotCharging
            && present0 && charge0 > 0 && battState0 === bsActive
            && VehicleStore.seatboxLock === slClosed
    }
    // AUX low charge: charge ≤ 25%, not charging, main present & active, seatbox closed
    readonly property bool auxLowChargeCondition: {
        // types always defined
        return AuxBatteryStore.charge <= 25
            && AuxBatteryStore.chargeStatus === acsNotCharging
            && present0 && charge0 > 0 && battState0 === bsActive
            && VehicleStore.seatboxLock === slClosed
    }
    // AUX low voltage: voltage < 11500mV, not charging, main present, seatbox closed
    readonly property bool auxLowVoltageCondition: {
        // types always defined
        return AuxBatteryStore.voltage < 11500
            && AuxBatteryStore.chargeStatus === acsNotCharging
            && present0
            && VehicleStore.seatboxLock === slClosed
    }
    // AUX critical voltage: voltage < 11000mV, main present, seatbox closed
    readonly property bool auxCriticalCondition: {
        // types always defined
        return AuxBatteryStore.voltage < 11000
            && present0
            && VehicleStore.seatboxLock === slClosed
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

    // =====================================================================
    // Battery 0 icon
    // =====================================================================
    BatteryIcon {
        charge: charge0
        batteryState: battState0
        present: present0
        isBattery0: true
        hasFault: hasFault0
        iconColor: batteryDisplay.iconColor
        isDark: batteryDisplay.isDark
    }

    // Battery 0 charge/range text
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: present0 ? (showAsRange ? rangeText(charge0, soh0) : charge0 + "%") : ""
        font.pixelSize: ThemeStore.fontBody
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        color: chargeLabelColor(charge0, true)
    }

    // Group separator before Battery 1
    Item { width: 5; height: 1; visible: batteryDisplay.showDual }

    // =====================================================================
    // Battery 1 icon (dual mode)
    // =====================================================================
    BatteryIcon {
        visible: batteryDisplay.showDual
        charge: charge1
        batteryState: battState1
        present: present1
        isBattery0: false
        hasFault: hasFault1
        iconColor: batteryDisplay.iconColor
        isDark: batteryDisplay.isDark
    }

    // Battery 1 charge/range text
    Text {
        anchors.verticalCenter: parent.verticalCenter
        visible: batteryDisplay.showDual
        text: present1 ? (showAsRange ? rangeText(charge1, soh1) : charge1 + "%") : ""
        font.pixelSize: ThemeStore.fontBody
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        color: chargeLabelColor(charge1, false)
    }

    // Group separator before warning icons
    Item { width: 5; height: 1; visible: batteryDisplay.seatboxOpen || showCbWarning || showAuxWarning || showTurtle }

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
            anchors.fill: parent
            source: isDark ? "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
                           : "qrc:/ScootUI/assets/icons/librescoot-overlay-error-light.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
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
            anchors.fill: parent
            source: isDark ? "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
                           : "qrc:/ScootUI/assets/icons/librescoot-overlay-error-light.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
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

import QtQuick
import QtQuick.Effects

Row {
    id: batteryDisplay
    spacing: 3
    height: 24

    Component.onCompleted: if (typeof bootTimer !== "undefined")
        console.log("[boot +" + bootTimer.elapsed() + "ms] BatteryDisplay completed")

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

    function chargeLabelColor(charge, battState) {
        if (battState === bsActive) {
            if (charge <= 10) return "#FF0000"
            if (charge <= 20) return "#FF7900"
        }
        return themeStore.textColor
    }

    // --- Optional CBB / AUX charge indicators (icon-only) ---
    // One setting per battery: visibility (always / warning / never, default
    // warning). "warning" means the pack reads low: CBB by SoC <= 50% (it has a
    // real fuel gauge), AUX by voltage (no gauge, see the aux thresholds below).
    // Detailed charge/voltage is available via the hold-both-brakes parked view,
    // so the status bar stays icon-only.
    readonly property string cbVisibility: typeof settingsStore !== "undefined" ? settingsStore.showCbBattery : "warning"
    readonly property string auxVisibility: typeof settingsStore !== "undefined" ? settingsStore.showAuxBattery : "warning"

    readonly property bool cbPresent: typeof cbBatteryStore !== "undefined" && cbBatteryStore.present
    readonly property int cbCharge: typeof cbBatteryStore !== "undefined" ? cbBatteryStore.charge : 0
    readonly property bool cbChargeValid: typeof cbBatteryStore !== "undefined" && cbBatteryStore.chargeValid
    readonly property bool cbLow: cbChargeValid && cbCharge <= 50

    readonly property int auxCharge: typeof auxBatteryStore !== "undefined" ? auxBatteryStore.charge : 0
    readonly property bool auxChargeValid: typeof auxBatteryStore !== "undefined" && auxBatteryStore.chargeValid
    readonly property int auxVoltageMv: typeof auxBatteryStore !== "undefined" ? auxBatteryStore.voltage : 0
    readonly property bool auxVoltageValid: typeof auxBatteryStore !== "undefined" && auxBatteryStore.voltageValid

    // AUX 12V thresholds, in mV. The AUX pack has no fuel gauge: mdb-nrf52
    // quantizes this same voltage into 5 SoC buckets (0/25/50/75/100), so SoC
    // carries strictly less information than the voltage it is derived from.
    // Drive every aux low/warning/critical decision from voltage; SoC is kept
    // only for the charge-level glyph. These three values are the one place to
    // shift for a future "aux chemistry = LiFePO4" setting (the firmware SoC
    // table is lead-acid-specific and can't be reused for LiFePO4's flat curve).
    // Tiers: 12000 ~ 50% SoC (soft "low": icon visibility + stranded mirror),
    //        11500 ~ firmware empty line (charging-system warning),
    //        11000 critical.
    readonly property int auxLowVoltageMv: 12000
    readonly property int auxWarnVoltageMv: 11500
    readonly property int auxCriticalVoltageMv: 11000

    readonly property bool auxLow: auxVoltageValid && auxVoltageMv < auxLowVoltageMv

    function visibleByMode(mode, low) {
        if (mode === "always") return true
        if (mode === "warning") return low
        return false
    }

    // The level glyph needs a known SoC to pick a bucket; warnings take over the
    // slot when active, so don't double up.
    readonly property bool showCbCharge: cbPresent && cbChargeValid
                                         && visibleByMode(cbVisibility, cbLow)
                                         && !showCbWarning && !showCbStranded
    readonly property bool showAuxCharge: auxChargeValid
                                          && visibleByMode(auxVisibility, auxLow)
                                          && !showAuxWarning && !showAuxStranded

    function levelBucket(charge) {
        var b = Math.round(charge / 25) * 25
        return b < 0 ? 0 : (b > 100 ? 100 : b)
    }

    // --- Seatbox ---
    readonly property bool seatboxOpen: typeof vehicleStore !== "undefined"
                                         ? vehicleStore.seatboxLock !== slClosed : false

    // --- Turtle mode ---
    readonly property bool showTurtle: (present0 && battState0 === bsActive && charge0 <= 20)
                                       || (present1 && battState1 === bsActive && charge1 <= 20)

    // --- Battery warning conditions ---
    // CB battery not present
    readonly property bool cbNotPresent: typeof cbBatteryStore !== "undefined" && !cbBatteryStore.present

    // CB warning: charge < 95%, not charging, main present & active, seatbox closed
    readonly property bool cbWarningCondition: {
        if (typeof cbBatteryStore === "undefined" || typeof vehicleStore === "undefined") return false
        if (!cbBatteryStore.present) return false
        return cbBatteryStore.charge < 95
            && cbBatteryStore.chargeStatus === csNotCharging
            && present0 && charge0 > 0 && battState0 === bsActive
            && vehicleStore.seatboxLock === slClosed
    }
    // AUX low voltage: not charging, main present, seatbox closed. Replaces the
    // old SoC <= 25% gate - voltage is the same signal without the bucketing.
    readonly property bool auxLowVoltageCondition: {
        if (typeof auxBatteryStore === "undefined" || typeof vehicleStore === "undefined") return false
        return auxVoltageValid && auxVoltageMv < auxWarnVoltageMv
            && auxBatteryStore.chargeStatus === acsNotCharging
            && present0
            && vehicleStore.seatboxLock === slClosed
    }
    // AUX critical voltage: main present, seatbox closed
    readonly property bool auxCriticalCondition: {
        if (typeof auxBatteryStore === "undefined" || typeof vehicleStore === "undefined") return false
        return auxVoltageValid && auxVoltageMv < auxCriticalVoltageMv
            && present0
            && vehicleStore.seatboxLock === slClosed
    }

    // --- "Stranded" warnings: backup battery low while NO main battery is inserted ---
    // Distinct from the charging-system warnings above (which require a main
    // battery present and active). These fire regardless of seatbox state.
    // Toasts for these are driven separately by the C++ BackupBatteryMonitor;
    // here we only mirror the conditions to drive the status-bar icons.
    readonly property bool noMainBattery: !present0 && !present1

    // CBB low and stranded: reuse the 95% SoC gate. Only act on a reported SoC -
    // "never received" is not a low reading.
    readonly property bool cbStrandedCondition: {
        if (typeof cbBatteryStore === "undefined") return false
        return noMainBattery && cbBatteryStore.present
            && cbBatteryStore.chargeValid && cbBatteryStore.charge < 95
    }
    // AUX low and stranded: low aux voltage while no main battery is inserted.
    // Mirrors the C++ BackupBatteryMonitor, which drives the actual toast.
    readonly property bool auxStrandedCondition: {
        if (typeof auxBatteryStore === "undefined") return false
        if (!noMainBattery) return false
        return auxVoltageValid && auxVoltageMv < auxLowVoltageMv
    }

    // --- 3-second debounce for warning indicators (matching Flutter) ---
    property bool showCbWarning: false
    property bool showAuxWarning: false
    property bool showCbStranded: false
    property bool showAuxStranded: false

    property bool _cbDebounceActive: false
    property bool _auxDebounceActive: false
    property bool _cbStrandedDebounceActive: false
    property bool _auxStrandedDebounceActive: false

    readonly property bool _anyAuxCondition: auxLowVoltageCondition || auxCriticalCondition

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

    onCbStrandedConditionChanged: {
        if (cbStrandedCondition) {
            if (!_cbStrandedDebounceActive) {
                _cbStrandedDebounceActive = true
                cbStrandedDebounceTimer.restart()
            }
        } else {
            _cbStrandedDebounceActive = false
            cbStrandedDebounceTimer.stop()
            showCbStranded = false
        }
    }

    onAuxStrandedConditionChanged: {
        if (auxStrandedCondition) {
            if (!_auxStrandedDebounceActive) {
                _auxStrandedDebounceActive = true
                auxStrandedDebounceTimer.restart()
            }
        } else {
            _auxStrandedDebounceActive = false
            auxStrandedDebounceTimer.stop()
            showAuxStranded = false
        }
    }

    Timer {
        id: cbStrandedDebounceTimer
        interval: 3000
        onTriggered: {
            if (batteryDisplay.cbStrandedCondition)
                batteryDisplay.showCbStranded = true
        }
    }

    Timer {
        id: auxStrandedDebounceTimer
        interval: 3000
        onTriggered: {
            if (batteryDisplay.auxStrandedCondition)
                batteryDisplay.showAuxStranded = true
        }
    }

    // =====================================================================
    // Battery 0 icon
    // =====================================================================
    BatteryIcon {
        charge: charge0
        batteryState: battState0
        present: present0
        hasFault: hasFault0
        iconColor: batteryDisplay.iconColor
        isDark: batteryDisplay.isDark
    }

    // Battery 0 charge/range text
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: present0 ? (showAsRange ? rangeText(charge0, soh0) : charge0 + "%") : ""
        font.pixelSize: themeStore.fontBody
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        color: chargeLabelColor(charge0, battState0)
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
        hasFault: hasFault1
        iconColor: batteryDisplay.iconColor
        isDark: batteryDisplay.isDark
    }

    // Battery 1 charge/range text
    Text {
        anchors.verticalCenter: parent.verticalCenter
        visible: batteryDisplay.showDual
        text: present1 ? (showAsRange ? rangeText(charge1, soh1) : charge1 + "%") : ""
        font.pixelSize: themeStore.fontBody
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        color: chargeLabelColor(charge1, battState1)
    }

    // =====================================================================
    // Optional CBB / AUX charge indicators (icon-only level glyph)
    // =====================================================================
    Item { width: 5; height: 1; visible: batteryDisplay.showCbCharge || batteryDisplay.showAuxCharge }

    Item {
        visible: batteryDisplay.showCbCharge
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            anchors.fill: parent
            source: batteryDisplay.showCbCharge
                    ? "qrc:/ScootUI/assets/icons/librescoot-cb-battery-level-"
                      + batteryDisplay.levelBucket(batteryDisplay.cbCharge) + ".svg"
                    : ""
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: batteryDisplay.iconColor
            }
        }
    }

    Item {
        visible: batteryDisplay.showAuxCharge
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            anchors.fill: parent
            source: batteryDisplay.showAuxCharge
                    ? "qrc:/ScootUI/assets/icons/librescoot-aux-battery-level-"
                      + batteryDisplay.levelBucket(batteryDisplay.auxCharge) + ".svg"
                    : ""
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: batteryDisplay.iconColor
            }
        }
    }

    // Group separator before warning icons
    Item { width: 5; height: 1; visible: batteryDisplay.seatboxOpen || cbNotPresent || showCbWarning || showAuxWarning || showCbStranded || showAuxStranded || showTurtle }

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
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: batteryDisplay.iconColor
            }
        }
    }

    // =====================================================================
    // CB battery not present (blank + slashed overlay)
    // =====================================================================
    Item {
        visible: cbNotPresent
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: cbAbsentIcon
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-cb-battery-blank.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: batteryDisplay.iconColor
            }
        }
        Image {
            anchors.fill: parent
            source: isDark ? "qrc:/ScootUI/assets/icons/librescoot-overlay-slashed.svg"
                           : "qrc:/ScootUI/assets/icons/librescoot-overlay-slashed-light.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
    }

    // =====================================================================
    // Battery warning indicators (CB and AUX with error overlay)
    // =====================================================================
    Item {
        visible: showCbWarning || showCbStranded
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: cbIcon
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-cb-battery-blank.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: batteryDisplay.iconColor
            }
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
        visible: showAuxWarning || showAuxStranded
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: auxIcon
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-aux-battery-blank.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: batteryDisplay.iconColor
            }
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
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: batteryDisplay.iconColor
            }
        }
    }
}

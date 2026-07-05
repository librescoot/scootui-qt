import QtQuick
import QtQuick.Effects
import "../components"

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

    // ChargeStatus: Charging=0, NotCharging=1, Unknown=2
    readonly property int csCharging: 0

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

    // "icon" hides the value text entirely, leaving just the battery icons.
    readonly property bool showText: typeof settingsStore === "undefined"
                                     || settingsStore.batteryDisplayMode !== "icon"

    function valueString(charge, soh, withDecimals) {
        if (!showAsRange)
            return charge.toString()
        var rangeKm = 45.0 * (soh / 100.0) * (charge / 100.0)
        if (rangeKm >= 10 || !withDecimals)
            return Math.floor(rangeKm).toString()
        return rangeKm.toFixed(1)
    }

    // Value sits a little smaller than the body font; the unit (km / %) is
    // smaller and lighter still, tucked close to the number to save width.
    readonly property real batteryValueSize: 16
    readonly property real batteryUnitSize: 13

    component BatteryValue: Row {
        property int charge: 0
        property real soh: 100
        property int battState: 0
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            id: valNum
            anchors.verticalCenter: parent.verticalCenter
            text: batteryDisplay.valueString(charge, soh, batteryDisplay.showDecimals)
            font.pixelSize: batteryDisplay.batteryValueSize
            font.weight: Font.DemiBold
            font.letterSpacing: -1.1
            color: batteryDisplay.chargeLabelColor(charge, battState)
        }
        Text {
            anchors.baseline: valNum.baseline
            text: batteryDisplay.showAsRange ? "km" : "%"
            font.pixelSize: batteryDisplay.batteryUnitSize
            font.weight: Font.Normal
            color: batteryDisplay.chargeLabelColor(charge, battState)
        }
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
    // warning). "warning" means the pack reads low: CBB by SoC < 50% (it has a
    // real fuel gauge), AUX by voltage (no gauge, see the aux thresholds below).
    // Detailed charge/voltage is available via the hold-both-brakes parked view,
    // so the status bar stays icon-only.
    readonly property string cbVisibility: typeof settingsStore !== "undefined" ? settingsStore.showCbBattery : "warning"
    readonly property string auxVisibility: typeof settingsStore !== "undefined" ? settingsStore.showAuxBattery : "warning"

    readonly property bool cbPresent: typeof cbBatteryStore !== "undefined" && cbBatteryStore.present
    readonly property int cbCharge: typeof cbBatteryStore !== "undefined" ? cbBatteryStore.charge : 0
    readonly property bool cbChargeValid: typeof cbBatteryStore !== "undefined" && cbBatteryStore.chargeValid
    readonly property bool cbLow: cbChargeValid && cbCharge < 50

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
    // Tiers: 11700 (soft "low": icon visibility + stranded mirror),
    //        11495 ~ firmware empty line (charging-system warning),
    //        11000 critical.
    readonly property int auxLowVoltageMv: 11700
    readonly property int auxWarnVoltageMv: 11495
    readonly property int auxCriticalVoltageMv: 11000

    readonly property bool auxLow: auxVoltageValid && auxVoltageMv < auxLowVoltageMv

    function visibleByMode(mode, low) {
        if (mode === "always") return true
        if (mode === "warning") return low
        return false
    }

    // The level glyph needs a known SoC to pick a bucket; warnings take over the
    // slot when active, so don't double up.
    readonly property bool showCbChargeWanted: cbPresent && cbChargeValid
                                         && visibleByMode(cbVisibility, cbLow)
                                         && !showCbWarning && !showCbStranded
    readonly property bool showAuxChargeWanted: auxChargeValid
                                          && visibleByMode(auxVisibility, auxLow)
                                          && !showAuxWarning && !showAuxStranded

    // --- Width-aware degradation -------------------------------------------
    // degradeLevel is assigned by TopStatusBar from the measured budget:
    //   0 full detail          3 AUX level glyph -> overflow chip
    //   1 no range decimals    4 CBB level glyph -> overflow chip
    //   2 no battery 1 value   5 no battery 0 value
    // Warning icons (seatbox, CB absent, CB/AUX error) never degrade.
    property int degradeLevel: 0

    readonly property bool showDecimals: degradeLevel < 1
    readonly property bool showB1Text: degradeLevel < 2
    readonly property bool showB0Text: degradeLevel < 5
    readonly property bool showCbCharge: showCbChargeWanted && degradeLevel < 4
    readonly property bool showAuxCharge: showAuxChargeWanted && degradeLevel < 3
    readonly property int chippedCount: (showAuxChargeWanted && degradeLevel >= 3 ? 1 : 0)
                                      + (showCbChargeWanted && degradeLevel >= 4 ? 1 : 0)

    // Width model: level widths are computed from TextMetrics and fixed icon
    // sizes, NOT from rendered children, so applying a degrade level can never
    // feed back into the measurement (no binding loops).
    TextMetrics {
        id: tmVal0Full
        font.pixelSize: batteryDisplay.batteryValueSize
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        text: batteryDisplay.valueString(batteryDisplay.charge0, batteryDisplay.soh0, true)
    }
    TextMetrics {
        id: tmVal0Short
        font.pixelSize: batteryDisplay.batteryValueSize
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        text: batteryDisplay.valueString(batteryDisplay.charge0, batteryDisplay.soh0, false)
    }
    TextMetrics {
        id: tmVal1Full
        font.pixelSize: batteryDisplay.batteryValueSize
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        text: batteryDisplay.valueString(batteryDisplay.charge1, batteryDisplay.soh1, true)
    }
    TextMetrics {
        id: tmVal1Short
        font.pixelSize: batteryDisplay.batteryValueSize
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        text: batteryDisplay.valueString(batteryDisplay.charge1, batteryDisplay.soh1, false)
    }
    TextMetrics {
        id: tmUnit
        font.pixelSize: batteryDisplay.batteryUnitSize
        font.weight: Font.Normal
        text: batteryDisplay.showAsRange ? "km" : "%"
    }

    function widthAtLevel(level) {
        var w = 0
        var n = 0
        function add(itemW) { w += (n > 0 ? spacing : 0) + itemW; n++ }

        add(24)                                               // battery 0 icon
        if (present0 && showText && level < 5)
            add((level >= 1 ? tmVal0Short.width : tmVal0Full.width) + 2 + tmUnit.width)
        if (showDual) {
            add(5)                                            // group separator
            add(24)                                           // battery 1 icon
            if (present1 && showText && level < 2)
                add((level >= 1 ? tmVal1Short.width : tmVal1Full.width) + 2 + tmUnit.width)
        }
        var cbG = showCbChargeWanted && level < 4
        var auxG = showAuxChargeWanted && level < 3
        var chips = (showAuxChargeWanted && level >= 3 ? 1 : 0)
                  + (showCbChargeWanted && level >= 4 ? 1 : 0)
        if (cbG || auxG || chips > 0) add(5)                  // group separator
        if (cbG) add(24)
        if (auxG) add(24)
        if (chips > 0) add(24)                                // overflow chip
        var warn = seatboxOpen || cbNotPresent || showCbWarning || showAuxWarning
                 || showCbStranded || showAuxStranded
        if (warn) add(5)                                      // group separator
        if (seatboxOpen) add(24)
        if (cbNotPresent) add(24)
        if (showCbWarning || showCbStranded) add(24)
        if (showAuxWarning || showAuxStranded) add(24)
        return w
    }

    readonly property var levelWidths: [
        widthAtLevel(0), widthAtLevel(1), widthAtLevel(2),
        widthAtLevel(3), widthAtLevel(4), widthAtLevel(5)
    ]

    function levelBucket(charge) {
        var b = Math.round(charge / 25) * 25
        return b < 0 ? 0 : (b > 100 ? 100 : b)
    }

    // --- Seatbox ---
    readonly property bool seatboxOpen: typeof vehicleStore !== "undefined"
                                         ? vehicleStore.seatboxLock !== slClosed : false

    // --- Battery warning conditions ---
    // CB battery not present
    readonly property bool cbNotPresent: typeof cbBatteryStore !== "undefined" && !cbBatteryStore.present

    // CB warning: charge < 50%, not charging, main present & active, seatbox closed
    readonly property bool cbWarningCondition: {
        if (typeof cbBatteryStore === "undefined" || typeof vehicleStore === "undefined") return false
        if (!cbBatteryStore.present) return false
        return cbBatteryStore.charge < 50
            && cbBatteryStore.chargeStatus !== csCharging
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

    // CBB low and stranded: reuse the 50% SoC gate. Only act on a reported SoC -
    // "never received" is not a low reading.
    readonly property bool cbStrandedCondition: {
        if (typeof cbBatteryStore === "undefined") return false
        return noMainBattery && cbBatteryStore.present
            && cbBatteryStore.chargeValid && cbBatteryStore.charge < 50
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
    BatteryValue {
        visible: present0 && batteryDisplay.showText && batteryDisplay.showB0Text
        charge: charge0
        soh: soh0
        battState: battState0
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
    BatteryValue {
        visible: batteryDisplay.showDual && present1 && batteryDisplay.showText && batteryDisplay.showB1Text
        charge: charge1
        soh: soh1
        battState: battState1
    }

    // =====================================================================
    // Optional CBB / AUX charge indicators (icon-only level glyph)
    // =====================================================================
    Item { width: 5; height: 1; visible: batteryDisplay.showCbCharge || batteryDisplay.showAuxCharge || batteryDisplay.chippedCount > 0 }

    Item {
        visible: batteryDisplay.showCbCharge
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: cbLevelIcon
            anchors.fill: parent
            source: batteryDisplay.showCbCharge
                    ? "qrc:/ScootUI/assets/icons/librescoot-cb-battery-level-"
                      + batteryDisplay.levelBucket(batteryDisplay.cbCharge) + ".svg"
                    : ""
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: cbLevelIcon
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
    }

    Item {
        visible: batteryDisplay.showAuxCharge
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: auxLevelIcon
            anchors.fill: parent
            source: batteryDisplay.showAuxCharge
                    ? "qrc:/ScootUI/assets/icons/librescoot-aux-battery-level-"
                      + batteryDisplay.levelBucket(batteryDisplay.auxCharge) + ".svg"
                    : ""
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: auxLevelIcon
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
        }
    }

    OverflowChip {
        count: batteryDisplay.chippedCount
        iconColor: batteryDisplay.iconColor
        anchors.verticalCenter: parent.verticalCenter
    }

    // Group separator before warning icons
    Item { width: 5; height: 1; visible: batteryDisplay.seatboxOpen || cbNotPresent || showCbWarning || showAuxWarning || showCbStranded || showAuxStranded }

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
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: seatboxIcon
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
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
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: cbAbsentIcon
            colorization: 1.0
            colorizationColor: batteryDisplay.iconColor
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
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: cbIcon
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
        visible: showAuxWarning || showAuxStranded
        width: 24; height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: auxIcon
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-aux-battery-blank.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: auxIcon
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

}

import QtQuick
import QtQuick.Layouts
import "../components"
import ScootUI 1.0

Item {
    id: powerDisplay

    readonly property real motorCurrent: typeof EngineStore !== "undefined" ? EngineStore.motorCurrent : 0
    readonly property real motorVoltage: typeof EngineStore !== "undefined" ? EngineStore.motorVoltage : 0
    readonly property bool ecuStale: typeof EngineStore !== "undefined" && EngineStore.faultCode === 20

    // Regen availability, derived by ecu-service. Default to available when the
    // store isn't present so the bar reads solid.
    readonly property bool regenAvailable: typeof EngineStore !== "undefined" ? EngineStore.regenAvailable : true
    readonly property string regenReason: typeof EngineStore !== "undefined" ? EngineStore.regenReason : "none"
    // A reason icon (cold/hot/full) is shown at the regen end of the bar. Other
    // off-reasons (user-disabled, standstill) still dash the track but carry no
    // icon — standstill is the normal state at every stop, so an icon there would
    // just flicker on and off.
    readonly property bool showReasonIcon: !regenAvailable
                                           && (regenReason === "cold" || regenReason === "hot"
                                               || regenReason === "full")
    readonly property color trackColor: ThemeStore.isDark ? "#424242" : "#E0E0E0"
    // Reason-icon colors, theme-reactive (bright in dark, deep in light) for
    // legible contrast on the grey regen track.
    readonly property color reasonColdColor: ThemeStore.isDark ? "#40C4FF" : "#0277BD" // blue
    readonly property color reasonHotColor:  ThemeStore.isDark ? "#FF6E40" : "#D84315" // deep orange
    readonly property color reasonFullColor: ThemeStore.isDark ? "#69F0AE" : "#2E7D32" // green

    // 0 = kW (default), 1 = Amps
    readonly property int displayMode: typeof SettingsStore !== "undefined" ? SettingsStore.powerDisplayMode : 0
    readonly property bool isAmpsMode: displayMode === 1

    // Current in A, Power in kW (voltage in mV × current in mA → W, /1e6 → kW)
    readonly property real currentA: motorCurrent / 1000
    readonly property real powerKw: (motorVoltage * motorCurrent) / 1000000000

    // Regen scale stays fixed (doubled for dual-battery setups).
    readonly property bool isDualBattery: typeof SettingsStore !== "undefined" && SettingsStore.dualBattery
    readonly property real maxRegenA: isDualBattery ? 20 : 10
    readonly property real maxRegenKw: isDualBattery ? 1.08 : 0.54
    // Boost = above the motor's rated continuous output.
    readonly property real boostThresholdA: isDualBattery ? 120 : 60
    readonly property real boostThresholdKw: isDualBattery ? 5.4 : 2.7

    // Discharge scale tracks the session high-water mark, with a floor so the
    // bar isn't underscaled before the first hard pull. Resets each session.
    readonly property real dischargeFloorA: 60
    readonly property real dischargeFloorKw: 3.0
    property real maxDischargeA: dischargeFloorA
    property real maxDischargeKw: dischargeFloorKw

    onCurrentAChanged: {
        if (!ecuStale && currentA > maxDischargeA) maxDischargeA = currentA
    }
    onPowerKwChanged: {
        if (!ecuStale && powerKw > maxDischargeKw) maxDischargeKw = powerKw
    }

    readonly property real rawValue: isAmpsMode ? currentA : powerKw
    readonly property real maxRegen: isAmpsMode ? maxRegenA : maxRegenKw
    readonly property real maxDischarge: isAmpsMode ? maxDischargeA : maxDischargeKw
    readonly property real boostThreshold: isAmpsMode ? boostThresholdA : boostThresholdKw
    readonly property string unit: isAmpsMode ? "A" : "kW"

    // Animated value
    property real displayValue: 0
    Behavior on displayValue {
        NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
    }
    onRawValueChanged: {
        if (Math.abs(rawValue - displayValue) > 0.01) {
            displayValue = rawValue
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item { Layout.fillHeight: true }

        // Labels
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: Translations.powerRegen
                font.pixelSize: ThemeStore.fontCaption
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: ThemeStore.textHint
                bottomPadding: -2
            }
            Item { Layout.fillWidth: true }
            Text {
                text: Translations.powerDischarge
                font.pixelSize: ThemeStore.fontCaption
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: ThemeStore.textHint
                bottomPadding: -2
            }
        }

        // Bar
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 10
            Layout.topMargin: 2

            // Discharge-side track (right of center): always solid.
            Rectangle {
                x: parent.width / 2
                width: parent.width / 2
                height: 6
                anchors.verticalCenter: parent.verticalCenter
                radius: ThemeStore.radiusBar
                color: powerDisplay.trackColor
            }

            // Regen-side track (left of center): solid when KERS is available,
            // dashed when regen is unavailable.
            Item {
                id: regenTrack
                x: 0
                width: parent.width / 2
                height: 6
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    anchors.fill: parent
                    radius: ThemeStore.radiusBar
                    color: powerDisplay.trackColor
                    visible: powerDisplay.regenAvailable
                }

                // Dashes packed toward the center, leaving room at the left end
                // for the reason icon when one is shown.
                Row {
                    id: dashRow
                    anchors.fill: parent
                    anchors.leftMargin: powerDisplay.showReasonIcon ? 14 : 0
                    layoutDirection: Qt.RightToLeft
                    spacing: 3
                    visible: !powerDisplay.regenAvailable
                    Repeater {
                        model: Math.max(1, Math.floor((dashRow.width + 3) / (5 + 3)))
                        Rectangle {
                            width: 5
                            height: 6
                            radius: ThemeStore.radiusBar
                            color: powerDisplay.trackColor
                        }
                    }
                }
            }

            // Regen-unavailable reason icon at the left (regen) end of the bar.
            Text {
                visible: powerDisplay.showReasonIcon
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    if (powerDisplay.regenReason === "hot") return MaterialIcon.iconHeat
                    if (powerDisplay.regenReason === "full") return MaterialIcon.iconBatteryFull
                    return MaterialIcon.iconSnowflake
                }
                font.family: "Material Icons"
                font.pixelSize: 12
                color: {
                    if (powerDisplay.regenReason === "hot") return powerDisplay.reasonHotColor
                    if (powerDisplay.regenReason === "full") return powerDisplay.reasonFullColor
                    return powerDisplay.reasonColdColor
                }
            }

            // Zero marker
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: 2
                height: 8
                color: ThemeStore.isDark ? "#66FFFFFF" : "#61000000"
            }

            // Regen bar (grows left from center)
            Rectangle {
                visible: powerDisplay.regenAvailable && displayValue < -0.01
                anchors.verticalCenter: parent.verticalCenter
                height: 6
                radius: ThemeStore.radiusBar
                width: Math.min(Math.abs(displayValue) / maxRegen, 1.0) * (parent.width / 2)
                x: parent.width / 2 - width
                color: "#43A047"
            }

            // Discharge bar (grows right from center)
            Rectangle {
                visible: displayValue > 0.01
                x: parent.width / 2
                anchors.verticalCenter: parent.verticalCenter
                height: 6
                radius: ThemeStore.radiusBar
                width: Math.min(displayValue / maxDischarge, 1.0) * (parent.width / 2)
                color: displayValue > boostThreshold ? "#FB8C00" : "#1E88E5"
            }
        }

        // Value text
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 1
            font.pixelSize: ThemeStore.fontBody
            color: ThemeStore.textHint
            text: {
                if (ecuStale) return "—"
                var absVal = Math.abs(displayValue)
                if (absVal < 0.01) return "0 " + unit
                if (isAmpsMode) return displayValue.toFixed(0) + " " + unit
                return displayValue.toFixed(1) + " " + unit
            }
        }

        Item { Layout.fillHeight: true }
    }
}

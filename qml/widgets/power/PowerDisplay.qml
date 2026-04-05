import QtQuick
import QtQuick.Layouts
import "../../theme"

Item {
    id: powerDisplay

    readonly property real motorCurrent: typeof engineStore !== "undefined" ? engineStore.motorCurrent : 0
    readonly property real motorVoltage: typeof engineStore !== "undefined" ? engineStore.motorVoltage : 0

    // 0 = kW (default), 1 = Amps
    readonly property int displayMode: typeof settingsStore !== "undefined" ? settingsStore.powerDisplayMode : 0
    readonly property bool isAmpsMode: displayMode === 1

    // Current in A, Power in kW (voltage in mV × current in mA → W, /1e6 → kW)
    readonly property real currentA: motorCurrent / 1000
    readonly property real powerKw: (motorVoltage * motorCurrent) / 1000000000

    // Display values
    readonly property real maxRegenA: 10
    readonly property real maxDischargeA: 80
    readonly property real boostThresholdA: 50
    readonly property real maxRegenKw: 0.54
    readonly property real maxDischargeKw: 4.0

    readonly property real rawValue: isAmpsMode ? currentA : powerKw
    readonly property real maxRegen: isAmpsMode ? maxRegenA : maxRegenKw
    readonly property real maxDischarge: isAmpsMode ? maxDischargeA : maxDischargeKw
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
                text: qsTr("Regen")
                font.pixelSize: Theme.fontBody
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: themeStore.textHint
                bottomPadding: -2
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("Discharge")
                font.pixelSize: Theme.fontBody
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: themeStore.textHint
                bottomPadding: -2
            }
        }

        // Bar
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 10
            Layout.topMargin: 2

            // Background bar
            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: 6
                radius: Theme.radiusBar
                color: themeStore.isDark ? "#424242" : "#E0E0E0"
            }

            // Zero marker
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: 2
                height: 8
                color: themeStore.isDark ? "#66FFFFFF" : "#61000000"
            }

            // Regen bar (grows left from center)
            Rectangle {
                visible: displayValue < -0.01
                anchors.verticalCenter: parent.verticalCenter
                height: 6
                radius: Theme.radiusBar
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
                radius: Theme.radiusBar
                width: Math.min(displayValue / maxDischarge, 1.0) * (parent.width / 2)
                color: isAmpsMode && displayValue > boostThresholdA ? "#FB8C00" : "#1E88E5"
            }
        }

        // Value text
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 1
            font.pixelSize: Theme.fontBody
            color: themeStore.textHint
            text: {
                var absVal = Math.abs(displayValue)
                if (absVal < 0.01) return "0 " + unit
                if (isAmpsMode) return displayValue.toFixed(0) + " " + unit
                return displayValue.toFixed(1) + " " + unit
            }
        }

        Item { Layout.fillHeight: true }
    }
}

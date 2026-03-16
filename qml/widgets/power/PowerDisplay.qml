import QtQuick
import QtQuick.Layouts

Item {
    id: powerDisplay

    readonly property real motorCurrent: typeof engineStore !== "undefined" ? engineStore.motorCurrent : 0
    readonly property real motorVoltage: typeof engineStore !== "undefined" ? engineStore.motorVoltage : 0

    // Power in kW
    readonly property real powerKw: (motorVoltage * motorCurrent) / 1000000000
    // Current in A
    readonly property real currentA: motorCurrent / 1000

    // Display values
    readonly property real maxRegenA: 10
    readonly property real maxDischargeA: 80
    readonly property real boostThresholdA: 50

    // Animated value
    property real displayValue: 0
    Behavior on displayValue {
        NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
    }
    onCurrentAChanged: {
        if (Math.abs(currentA - displayValue) > 0.01) {
            displayValue = currentA
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 4
        spacing: 0

        // Labels
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("Regen")
                font.pixelSize: 10
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: themeStore.textHint
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("Discharge")
                font.pixelSize: 10
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: themeStore.textHint
            }
        }

        // Bar
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            Layout.topMargin: 4

            // Background bar
            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: 4
                radius: 2
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
                visible: displayValue < -0.1
                anchors.right: parent.horizontalCenter > 0 ? undefined : parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 4
                radius: 2
                width: Math.min(Math.abs(displayValue) / maxRegenA, 1.0) * (parent.width / 2)
                x: parent.width / 2 - width
                color: "#43A047"
            }

            // Discharge bar (grows right from center)
            Rectangle {
                visible: displayValue > 0.1
                x: parent.width / 2
                anchors.verticalCenter: parent.verticalCenter
                height: 4
                radius: 2
                width: Math.min(displayValue / maxDischargeA, 1.0) * (parent.width / 2)
                color: displayValue > boostThresholdA ? "#FB8C00" : "#1E88E5"
            }
        }

        // Value text
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            font.pixelSize: 12
            color: themeStore.textHint
            text: {
                if (Math.abs(displayValue) < 0.1) return "0 A"
                return displayValue.toFixed(0) + " A"
            }
        }
    }
}

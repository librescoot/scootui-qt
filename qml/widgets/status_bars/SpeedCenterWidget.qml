import QtQuick
import QtQuick.Layouts
import "../indicators"

Item {
    id: speedCenter
    implicitWidth: 120
    implicitHeight: 75

    anchors.horizontalCenter: parent.horizontalCenter

    readonly property real speed: {
        if (typeof settingsStore !== "undefined" && typeof engineStore !== "undefined") {
            if (settingsStore.showRawSpeed && engineStore.hasRawSpeed)
                return engineStore.rawSpeed
        }
        return typeof engineStore !== "undefined" ? engineStore.speed : 0
    }

    // Speed display (centered)
    Column {
        anchors.centerIn: parent
        spacing: 0

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.floor(speed).toString()
            font.pixelSize: 48
            font.bold: true
            color: themeStore.textColor
            lineHeight: 1.0
            lineHeightMode: Text.ProportionalHeight
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "km/h"
            font.pixelSize: 16
            color: themeStore.textSecondary
            lineHeight: 0.8
            lineHeightMode: Text.ProportionalHeight
        }
    }

    // Speed limit indicator (right side, matching Flutter's Positioned(right: 0))
    SpeedLimitIndicator {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        iconSize: 31
    }
}

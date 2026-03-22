import QtQuick
import QtQuick.Layouts
import "../indicators"

Item {
    id: speedCenter
    implicitWidth: 120
    implicitHeight: 66

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
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.floor(speed).toString()
            font.pixelSize: 48
            font.bold: true
            color: themeStore.textColor
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "km/h"
            font.pixelSize: 16
            color: themeStore.textSecondary
        }
    }

    // Speed limit indicator (right side, matching Flutter's Positioned(right: 0))
    SpeedLimitIndicator {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        iconSize: 31
    }
}

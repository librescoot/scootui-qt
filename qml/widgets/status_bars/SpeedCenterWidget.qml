import QtQuick
import QtQuick.Layouts
import "../indicators"

Item {
    id: speedCenter
    implicitWidth: 120
    implicitHeight: speedColumn.implicitHeight

    anchors.horizontalCenter: parent.horizontalCenter

    readonly property real speed: {
        if (typeof settingsStore !== "undefined" && typeof engineStore !== "undefined") {
            if (settingsStore.showRawSpeed && engineStore.hasRawSpeed)
                return engineStore.rawSpeed
        }
        return typeof engineStore !== "undefined" ? engineStore.speed : 0
    }

    FontMetrics { id: speedFm; font: speedText.font }
    FontMetrics { id: unitFm; font: unitText.font }

    // Speed display (centered)
    Column {
        id: speedColumn
        anchors.centerIn: parent
        spacing: 2

        Text {
            id: speedText
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.floor(speed).toString()
            font.pixelSize: 48
            font.bold: true
            color: themeStore.textColor
            lineHeight: speedFm.ascent
            lineHeightMode: Text.FixedHeight
        }

        Text {
            id: unitText
            anchors.horizontalCenter: parent.horizontalCenter
            text: "km/h"
            font.pixelSize: 16
            color: themeStore.textSecondary
            lineHeight: unitFm.ascent
            lineHeightMode: Text.FixedHeight
        }
    }

    // Speed limit indicator (right side, matching Flutter's Positioned(right: 0))
    SpeedLimitIndicator {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        iconSize: 31
    }
}

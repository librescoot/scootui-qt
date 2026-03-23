import QtQuick
import QtQuick.Layouts
import "../indicators"

Item {
    id: speedCenter
    implicitWidth: 120
    implicitHeight: speedTight.tightBoundingRect.height + 2 + unitTight.tightBoundingRect.height

    anchors.horizontalCenter: parent.horizontalCenter

    readonly property real speed: {
        if (typeof settingsStore !== "undefined" && typeof engineStore !== "undefined") {
            if (settingsStore.showRawSpeed && engineStore.hasRawSpeed)
                return engineStore.rawSpeed
        }
        return typeof engineStore !== "undefined" ? engineStore.speed : 0
    }

    // Tight bounding rect metrics for pixel-perfect sizing
    TextMetrics {
        id: speedTight
        font: speedText.font
        text: speedText.text
    }
    TextMetrics {
        id: unitTight
        font: unitText.font
        text: unitText.text
    }

    // Speed display (centered)
    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            id: speedText
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.floor(speed).toString()
            font.pixelSize: 48
            font.bold: true
            color: themeStore.textColor
            height: speedTight.tightBoundingRect.height
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            id: unitText
            anchors.horizontalCenter: parent.horizontalCenter
            text: "km/h"
            font.pixelSize: 16
            color: themeStore.textSecondary
            height: unitTight.tightBoundingRect.height
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Speed limit indicator (right side, matching Flutter's Positioned(right: 0))
    SpeedLimitIndicator {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        iconSize: 31
    }
}

import QtQuick
import QtQuick.Layouts
import "../indicators"
import ScootUI

Item {
    id: speedCenter
    implicitWidth: 120
    implicitHeight: 4 + speedTight.tightBoundingRect.height + 2 + unitTight.tightBoundingRect.height + 2


    readonly property real speed: {
        if (SettingsStore.showRawSpeed && EngineStore.hasRawSpeed)
            return EngineStore.rawSpeed
        return EngineStore.speed
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
        id: speedCol
        anchors.centerIn: parent
        topPadding: 4
        bottomPadding: 2
        spacing: 2

        Text {
            id: speedText
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.floor(speed).toString()
            font.pixelSize: ThemeStore.fontXL
            font.weight: Font.Bold
            color: ThemeStore.textColor
            height: speedTight.tightBoundingRect.height
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            id: unitText
            anchors.horizontalCenter: parent.horizontalCenter
            text: "km/h"
            font.pixelSize: ThemeStore.fontBody
            color: ThemeStore.textSecondary
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

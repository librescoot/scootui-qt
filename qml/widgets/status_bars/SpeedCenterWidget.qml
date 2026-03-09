import QtQuick

Item {
    id: speedCenter
    implicitWidth: 120
    implicitHeight: 60

    readonly property real speed: {
        if (typeof settingsStore !== "undefined" && typeof engineStore !== "undefined") {
            if (settingsStore.showRawSpeed && engineStore.hasRawSpeed)
                return engineStore.rawSpeed
        }
        return typeof engineStore !== "undefined" ? engineStore.speed : 0
    }

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
            lineHeightMode: Text.FixedHeight
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "km/h"
            font.pixelSize: 16
            color: themeStore.textColor
            lineHeight: 0.8
            lineHeightMode: Text.ProportionalHeight
        }
    }

    // Speed limit indicator
    Image {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 31
        height: 31
        sourceSize: Qt.size(31, 31)
        visible: typeof speedLimitStore !== "undefined" && speedLimitStore.speedLimit !== ""
        source: "qrc:/ScootUI/assets/icons/speedlimit_blank.svg"

        Text {
            anchors.centerIn: parent
            text: typeof speedLimitStore !== "undefined" ? speedLimitStore.speedLimit : ""
            font.pixelSize: 15
            font.family: "Roboto Condensed"
            font.bold: true
            color: "black"
        }
    }
}

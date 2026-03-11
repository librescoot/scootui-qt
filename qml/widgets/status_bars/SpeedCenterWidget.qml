import QtQuick
import QtQuick.Layouts

Item {
    id: speedCenter
    implicitWidth: 120
    implicitHeight: 75

    // Horizontal alignment to match Flutter's Positional(right: 0) behavior
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottomMargin: 40

    readonly property real speed: {
        if (typeof settingsStore !== "undefined" && typeof engineStore !== "undefined") {
            if (settingsStore.showRawSpeed && engineStore.hasRawSpeed)
                return engineStore.rawSpeed
        }
        return typeof engineStore !== "undefined" ? engineStore.speed : 0
    }

    // Horizontal layout matching Flutter's Row with Expanded widgets
    RowLayout {
        anchors.fill: parent
        spacing: 20

        Item { Layout.fillWidth: true; Layout.preferredWidth: 20 }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 0

            Text {
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                text: Math.floor(speed).toString()
                font.pixelSize: 48
                font.bold: true
                color: themeStore.textColor
                lineHeight: 1.0
                lineHeightMode: Text.FixedHeight
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                text: "km/h"
                font.pixelSize: 16
                color: themeStore.textSecondary
                lineHeight: 0.8
                lineHeightMode: Text.FixedHeight
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredWidth: 20 }
    }
}
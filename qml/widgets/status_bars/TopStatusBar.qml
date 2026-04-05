import QtQuick
import QtQuick.Layouts
import "../indicators"

Rectangle {
    id: topBar
    color: "transparent"
    height: 40

    // Bottom border
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: themeStore.borderColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 0

        // Left: Battery display
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 2

            BatteryDisplay {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Center: Clock
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            visible: typeof settingsStore === "undefined" || settingsStore.showClock !== "never"

            Text {
                id: clockText
                anchors.centerIn: parent
                font.pixelSize: themeStore.fontHeading
                font.weight: Font.Medium
                color: themeStore.textColor

                property string timeStr: ""
                Timer {
                    interval: 1000
                    running: true
                    repeat: true
                    triggeredOnStart: true
                    onTriggered: {
                        var now = new Date()
                        var h = now.getHours().toString().padStart(2, '0')
                        var m = now.getMinutes().toString().padStart(2, '0')
                        clockText.timeStr = h + ":" + m
                    }
                }
                text: timeStr
            }
        }

        // Right: Status indicators
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 2

            StatusIndicators {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}

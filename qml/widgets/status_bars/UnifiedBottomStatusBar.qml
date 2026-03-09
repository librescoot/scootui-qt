import QtQuick
import QtQuick.Layouts

Rectangle {
    id: bottomBar
    color: "transparent"
    implicitHeight: 48

    readonly property real tripDistance: typeof tripStore !== "undefined" ? tripStore.distance : 0
    readonly property int tripDuration: typeof tripStore !== "undefined" ? tripStore.duration : 0
    readonly property real avgSpeed: typeof tripStore !== "undefined" ? tripStore.averageSpeed : 0
    readonly property real odometer: typeof engineStore !== "undefined" ? engineStore.odometer : 0

    // Top border
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: themeStore.borderColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 4

        // Left: Duration + Avg Speed
        Column {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            Row {
                spacing: 4
                Text {
                    text: "Duration"
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    font.letterSpacing: 0.5
                    color: themeStore.textHint
                }
                Text {
                    text: formatDuration(tripDuration)
                    font.pixelSize: 16
                    font.bold: true
                    color: themeStore.textColor
                }
            }
            Row {
                spacing: 4
                Text {
                    text: "Avg"
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    font.letterSpacing: 0.5
                    color: themeStore.textHint
                }
                Text {
                    text: Math.round(avgSpeed).toString()
                    font.pixelSize: 16
                    font.bold: true
                    color: themeStore.textColor
                }
                Text {
                    text: "km/h"
                    font.pixelSize: 12
                    color: themeStore.textHint
                    anchors.baseline: parent.children[1] ? parent.children[1].baseline : undefined
                }
            }
        }

        // Center: configurable widget (empty for now)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        // Right: Trip + Total distances
        Column {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            Row {
                anchors.right: parent.right
                spacing: 4
                Text {
                    text: "Trip"
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    color: themeStore.textHint
                }
                Text {
                    text: (tripDistance).toFixed(1)
                    font.pixelSize: 16
                    font.bold: true
                    color: themeStore.textColor
                }
                Text {
                    text: "km"
                    font.pixelSize: 12
                    color: themeStore.textHint
                }
            }
            Row {
                anchors.right: parent.right
                spacing: 4
                Text {
                    text: "Total"
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    color: themeStore.textHint
                }
                Text {
                    text: (odometer / 1000).toFixed(1)
                    font.pixelSize: 16
                    font.bold: true
                    color: themeStore.textColor
                }
                Text {
                    text: "km"
                    font.pixelSize: 12
                    color: themeStore.textHint
                }
            }
        }
    }

    function formatDuration(totalSeconds) {
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor((totalSeconds % 3600) / 60)
        var seconds = totalSeconds % 60
        if (hours > 0) {
            return hours + ":" + minutes.toString().padStart(2, '0')
        }
        return minutes.toString().padStart(2, '0') + ":" + seconds.toString().padStart(2, '0')
    }
}

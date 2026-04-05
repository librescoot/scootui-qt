import QtQuick
import QtQuick.Layouts

Rectangle {
    id: bottomBar
    color: typeof themeStore !== "undefined" ? themeStore.backgroundColor : "black"
    implicitHeight: Math.max(leftCol.height, centerItem.childrenRect.height, rightCol.height) + 8

    default property alias centerContent: centerItem.data

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

    // Left side
    Row {
        id: leftCol
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        anchors.bottomMargin: 2
        spacing: 16

        Column {
            spacing: 0
            Text {
                text: "Duration"
                font.pixelSize: themeStore.fontCaption
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
            }
            Text {
                text: formatDuration(tripDuration)
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
                color: themeStore.textColor
            }
        }

        Column {
            spacing: 0
            Text {
                text: "Avg"
                font.pixelSize: themeStore.fontCaption
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
            }
            Row {
                spacing: 2
                Text {
                    id: avgValue
                    text: avgSpeed.toFixed(1)
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Bold
                    color: themeStore.textColor
                }
                Text {
                    text: "km/h"
                    font.pixelSize: themeStore.fontCaption
                    color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
                    anchors.baseline: avgValue.baseline
                }
            }
        }
    }

    // Center: configurable widget
    Item {
        id: centerItem
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        width: childrenRect.width
        height: childrenRect.height
    }

    // Right side
    Row {
        id: rightCol
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 4
        anchors.bottomMargin: 2
        spacing: 16

        Column {
            spacing: 0
            Text {
                anchors.right: parent.right
                text: "Trip"
                font.pixelSize: themeStore.fontCaption
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
            }
            Text {
                anchors.right: parent.right
                text: (tripDistance).toFixed(1)
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
                color: themeStore.textColor
            }
        }

        Column {
            spacing: 0
            Text {
                anchors.right: parent.right
                text: "Total"
                font.pixelSize: themeStore.fontCaption
                font.weight: Font.Medium
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
            }
            Row {
                anchors.right: parent.right
                spacing: 2
                Text {
                    id: totalValue
                    text: (odometer / 1000).toFixed(1)
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Bold
                    color: themeStore.textColor
                }
                Text {
                    text: "km"
                    font.pixelSize: themeStore.fontCaption
                    color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
                    anchors.baseline: totalValue.baseline
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

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: bottomBar
    color: typeof themeStore !== "undefined" ? themeStore.backgroundColor : "black"
    implicitHeight: Math.max(48, centerItem.childrenRect.height + 16)

    // Allow injecting a center widget (like Flutter's centerWidget parameter)
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

    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 0

        // Left: Duration + Avg Speed (label above value, columns side by side)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 16

                // Duration column
                Column {
                    spacing: 0
                    Text {
                        text: "Duration"
                        font.pixelSize: 10
                        font.weight: Font.Medium
                        font.letterSpacing: 0.5
                        font.capitalization: Font.AllUppercase
                        color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
                    }
                    Text {
                        text: formatDuration(tripDuration)
                        font.pixelSize: 16
                        font.bold: true
                        color: themeStore.textColor
                    }
                }

                // Avg Speed column
                Column {
                    spacing: 0
                    Text {
                        text: "Avg"
                        font.pixelSize: 10
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
                            font.pixelSize: 16
                            font.bold: true
                            color: themeStore.textColor
                        }
                        Text {
                            text: "km/h"
                            font.pixelSize: 12
                            color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
                            anchors.baseline: avgValue.baseline
                        }
                    }
                }
            }
        }

        // Center: configurable widget
        Item {
            id: centerItem
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        // Right: Trip + Total distances (label above value, columns side by side)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 16

                // Trip column
                Column {
                    spacing: 0
                    Text {
                        anchors.right: parent.right
                        text: "Trip"
                        font.pixelSize: 10
                        font.weight: Font.Medium
                        font.letterSpacing: 0.5
                        font.capitalization: Font.AllUppercase
                        color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
                    }
                    Text {
                        anchors.right: parent.right
                        text: (tripDistance).toFixed(1)
                        font.pixelSize: 16
                        font.bold: true
                        color: themeStore.textColor
                    }
                }

                // Total column
                Column {
                    spacing: 0
                    Text {
                        anchors.right: parent.right
                        text: "Total"
                        font.pixelSize: 10
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
                            font.pixelSize: 16
                            font.bold: true
                            color: themeStore.textColor
                        }
                        Text {
                            text: "km"
                            font.pixelSize: 12
                            color: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
                            anchors.baseline: totalValue.baseline
                        }
                    }
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

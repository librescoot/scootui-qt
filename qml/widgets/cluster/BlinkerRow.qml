import QtQuick
import ScootUI 1.0

Item {
    id: blinkerRow
    height: 56

    readonly property int blinkerState: typeof vehicleStore !== "undefined" ? vehicleStore.blinkerState : Scooter.BlinkerState.Off

    readonly property bool showLeft: blinkerState === Scooter.BlinkerState.Left || blinkerState === Scooter.BlinkerState.Both
    readonly property bool showRight: blinkerState === Scooter.BlinkerState.Right || blinkerState === Scooter.BlinkerState.Both

    readonly property bool overlayEnabled: typeof settingsStore !== "undefined"
                                           ? settingsStore.blinkerStyle === "overlay" : false

    // Hide small blinkers if the large overlay is showing them instead
    visible: !overlayEnabled || (blinkerState !== Scooter.BlinkerState.Left && blinkerState !== Scooter.BlinkerState.Right)

    // Shared blink clock from VehicleStore
    readonly property real blinkOpacity: typeof vehicleStore !== "undefined" ? vehicleStore.blinkOpacity : 0

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true

    Row {
        anchors.fill: parent

        // Left blinker
        Item {
            width: 56
            height: 56

            Rectangle {
                anchors.centerIn: parent
                width: 52
                height: 52
                radius: 26
                color: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                border.width: 1
                border.color: isDark ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
                visible: showLeft
                opacity: Math.max(0.3, blinkerRow.blinkOpacity)

                Image {
                    anchors.centerIn: parent
                    width: 36
                    height: 36
                    source: "qrc:/ScootUI/assets/icons/librescoot-turn-left.svg"
                    sourceSize: Qt.size(36, 36)
                    opacity: blinkerRow.blinkOpacity / Math.max(0.3, blinkerRow.blinkOpacity)
                }
            }
        }

        // Spacer
        Item { width: parent.width - 112; height: 1 }

        // Right blinker
        Item {
            width: 56
            height: 56

            Rectangle {
                anchors.centerIn: parent
                width: 52
                height: 52
                radius: 26
                color: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                border.width: 1
                border.color: isDark ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
                visible: showRight
                opacity: Math.max(0.3, blinkerRow.blinkOpacity)

                Image {
                    anchors.centerIn: parent
                    width: 36
                    height: 36
                    source: "qrc:/ScootUI/assets/icons/librescoot-turn-right.svg"
                    sourceSize: Qt.size(36, 36)
                    opacity: blinkerRow.blinkOpacity / Math.max(0.3, blinkerRow.blinkOpacity)
                }
            }
        }
    }
}

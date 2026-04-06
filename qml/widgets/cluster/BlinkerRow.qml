import QtQuick
import ScootUI

Item {
    id: blinkerRow
    height: 56

    readonly property int blinkerState: VehicleStore.blinkerState
    // BlinkerState enum: 0=Off, 1=Left, 2=Right, 3=Both

    readonly property bool showLeft: blinkerState === 1 || blinkerState === 3
    readonly property bool showRight: blinkerState === 2 || blinkerState === 3

    readonly property bool overlayEnabled: SettingsStore.blinkerStyle === "overlay"

    // Hide small blinkers if large overlay is showing (state 1 or 2 with overlay enabled)
    visible: !overlayEnabled || (blinkerState !== 1 && blinkerState !== 2)

    // Shared blink clock from VehicleStore
    readonly property real blinkOpacity: VehicleStore.blinkOpacity

    readonly property bool isDark: ThemeStore.isDark

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

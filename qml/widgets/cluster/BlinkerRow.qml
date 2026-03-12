import QtQuick

Item {
    id: blinkerRow
    height: 56

    readonly property int blinkerState: typeof vehicleStore !== "undefined" ? vehicleStore.blinkerState : 0
    // BlinkerState enum: 0=Off, 1=Left, 2=Right, 3=Both

    readonly property bool showLeft: blinkerState === 1 || blinkerState === 3
    readonly property bool showRight: blinkerState === 2 || blinkerState === 3

    readonly property bool overlayEnabled: typeof settingsStore !== "undefined"
                                           ? settingsStore.blinkerStyle === "overlay" : false

    // Hide small blinkers if large overlay is showing (state 1 or 2 with overlay enabled)
    visible: !overlayEnabled || (blinkerState !== 1 && blinkerState !== 2)

    Row {
        anchors.fill: parent

        // Left blinker
        Item {
            width: 56
            height: 56

            Image {
                anchors.centerIn: parent
                width: 44
                height: 44
                source: "qrc:/ScootUI/assets/icons/librescoot-turn-left.svg"
                sourceSize: Qt.size(44, 44)
                visible: showLeft

                SequentialAnimation on opacity {
                    id: leftBlinkAnim
                    running: showLeft
                    loops: Animation.Infinite
                    NumberAnimation { from: 0; to: 1; duration: 250; easing.type: Easing.InOutExpo }
                    NumberAnimation { from: 1; to: 0; duration: 250; easing.type: Easing.InOutExpo }
                    PauseAnimation { duration: 228 }
                }
            }
        }

        // Spacer
        Item { width: parent.width - 112; height: 1 }

        // Right blinker
        Item {
            width: 56
            height: 56

            Image {
                anchors.centerIn: parent
                width: 44
                height: 44
                source: "qrc:/ScootUI/assets/icons/librescoot-turn-right.svg"
                sourceSize: Qt.size(44, 44)
                visible: showRight

                SequentialAnimation on opacity {
                    id: rightBlinkAnim
                    running: showRight
                    loops: Animation.Infinite
                    NumberAnimation { from: 0; to: 1; duration: 250; easing.type: Easing.InOutExpo }
                    NumberAnimation { from: 1; to: 0; duration: 250; easing.type: Easing.InOutExpo }
                    PauseAnimation { duration: 228 }
                }
            }
        }
    }
}

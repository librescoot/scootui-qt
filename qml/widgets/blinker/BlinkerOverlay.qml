import QtQuick

Item {
    id: blinkerOverlay
    anchors.fill: parent

    readonly property int blinkerState: typeof vehicleStore !== "undefined" ? vehicleStore.blinkerState : 0
    readonly property bool overlayEnabled: typeof settingsStore !== "undefined"
                                           ? settingsStore.blinkerStyle === "overlay" : false
    readonly property bool showLeft: overlayEnabled && blinkerState === 1
    readonly property bool showRight: overlayEnabled && blinkerState === 2
    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true

    visible: showLeft || showRight

    // Large centered arrow icon matching Flutter's 360x360 layout
    Item {
        anchors.centerIn: parent
        width: 360
        height: 360

        // Inactive base layer (faint arrow)
        Image {
            anchors.fill: parent
            source: showLeft
                    ? "qrc:/ScootUI/assets/icons/librescoot-turn-left.svg"
                    : "qrc:/ScootUI/assets/icons/librescoot-turn-right.svg"
            sourceSize: Qt.size(360, 360)
            fillMode: Image.PreserveAspectFit
            opacity: isDark ? 0.12 : 0.12
        }

        // Active animated layer (green arrow)
        Image {
            id: activeArrow
            anchors.fill: parent
            source: showLeft
                    ? "qrc:/ScootUI/assets/icons/librescoot-turn-left.svg"
                    : "qrc:/ScootUI/assets/icons/librescoot-turn-right.svg"
            sourceSize: Qt.size(360, 360)
            fillMode: Image.PreserveAspectFit
            opacity: 0

            SequentialAnimation {
                id: blinkAnim
                running: blinkerOverlay.visible
                loops: Animation.Infinite
                NumberAnimation {
                    target: activeArrow; property: "opacity"
                    from: 0; to: 0.8; duration: 250
                    easing.type: Easing.InOutExpo
                }
                NumberAnimation {
                    target: activeArrow; property: "opacity"
                    from: 0.8; to: 0; duration: 250
                    easing.type: Easing.InOutExpo
                }
                PauseAnimation { duration: 228 }
            }
        }
    }
}

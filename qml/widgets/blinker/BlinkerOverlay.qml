import QtQuick
import ScootUI

Item {
    id: blinkerOverlay
    anchors.fill: parent

    readonly property int blinkerState: VehicleStore.blinkerState
    readonly property bool overlayEnabled: SettingsStore.blinkerStyle === "overlay"
    readonly property bool showLeft: overlayEnabled && blinkerState === 1
    readonly property bool showRight: overlayEnabled && blinkerState === 2
    readonly property bool isDark: ThemeStore.isDark

    // Insets to center the arrow on the content area between status bars
    property real topInset: 0
    property real bottomInset: 0

    visible: showLeft || showRight

    // Shared blink clock from VehicleStore (scaled to 0.8 max)
    readonly property real blinkOpacity: VehicleStore.blinkOpacity * 0.8

    // Large arrow icon centered on the content area between status bars
    Item {
        anchors.horizontalCenter: parent.horizontalCenter
        y: topInset + (parent.height - topInset - bottomInset - height) / 2
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
            opacity: blinkerOverlay.blinkOpacity
        }
    }
}

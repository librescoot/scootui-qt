import QtQuick
import ScootUI 1.0

Item {
    id: serviceModeOverlay
    anchors.fill: parent
    visible: typeof SettingsStore !== "undefined" && SettingsStore.serviceActive === "true"

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 24
        color: "#FF6D00"
        opacity: 0.92

        Text {
            anchors.centerIn: parent
            text: typeof Translations !== "undefined" ? Translations.serviceModeActive : "Service Mode Active"
            color: "white"
            font.bold: true
            font.pixelSize: 12
        }
    }
}

import QtQuick

Item {
    id: serviceModeOverlay
    anchors.fill: parent
    visible: typeof settingsStore !== "undefined" && settingsStore.serviceActive === "true"

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 24
        color: "#FF6D00"
        opacity: 0.92

        Text {
            anchors.centerIn: parent
            text: typeof translations !== "undefined" ? translations.serviceModeActive : "Service Mode Active"
            color: "white"
            font.bold: true
            font.pixelSize: 12
        }
    }
}

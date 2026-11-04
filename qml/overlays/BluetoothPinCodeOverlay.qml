import QtQuick

Item {
    id: btPinOverlay
    anchors.fill: parent

    property string currentPin: ""

    property bool hopOn: typeof vehicleStore !== "undefined" && vehicleStore.hopOnActive

    visible: currentPin !== "" && !hopOn

    function syncPin() {
        currentPin = typeof bluetoothStore !== "undefined" ? bluetoothStore.pinCode : ""
        if (currentPin !== "")
            dismissTimer.restart()
        else
            dismissTimer.stop()
    }

    Component.onCompleted: syncPin()

    Connections {
        target: typeof bluetoothStore !== "undefined" ? bluetoothStore : null
        function onPinCodeChanged() { btPinOverlay.syncPin() }
    }

    Timer {
        id: dismissTimer
        objectName: "bluetoothPinExpiry"
        interval: 30000
        repeat: false
        onTriggered: btPinOverlay.currentPin = ""
    }

    // Bottom panel
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * 0.3
        color: "#2196F3"
        opacity: 0.8

        Column {
            anchors.centerIn: parent
            anchors.margins: 16
            spacing: 4

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: translations.blePinPrompt
                font.pixelSize: themeStore.fontTitle
                color: "#FFFFFF"
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: btPinOverlay.currentPin
                font.pixelSize: themeStore.fontPin
                font.weight: Font.Bold
                font.letterSpacing: 14
                color: "#FFFFFF"
            }
        }
    }
}

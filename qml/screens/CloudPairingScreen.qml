import QtQuick
import "../widgets/components"

Rectangle {
    id: root
    color: themeStore.isDark ? "black" : "white"
    readonly property color foreground: themeStore.isDark ? "white" : "black"
    readonly property color muted: themeStore.isDark ? "#aaaaaa" : "#555555"
    property double now: Date.now() / 1000
    readonly property string pairingState: cloudPairingService.state
    readonly property string stateText: {
        switch (pairingState) {
        case "connected": return translations.pairingConnected
        case "starting": return translations.pairingStarting
        case "waiting_for_web_setup": return translations.pairingApprove
        case "applying": return translations.pairingApplying
        case "stopping": return translations.pairingStopping
        case "paused": return translations.pairingPaused
        case "not_registered": case "registered": return translations.pairingReady
        default: return translations.pairingUnavailable
        }
    }
    function close() {
        screenStore.closeCloudPairing()
        menuStore.resume()
    }
    Connections {
        target: inputHandler
        function onLeftHold() { root.close() }
        function onRightTap() {
            if (cloudPairingService.canStop) cloudPairingService.stop()
            else if (cloudPairingService.canStart) cloudPairingService.start()
        }
    }
    Timer {
        interval: 1000
        repeat: true
        running: cloudPairingService.code !== ""
        onTriggered: root.now = Date.now() / 1000
    }
    Column {
        id: content
        objectName: "pairingContent"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 12
        Text {
            objectName: "pairingTitle"
            width: parent.width
            text: translations.pairingTitle
            color: root.foreground
            font.pixelSize: 28
            font.bold: true
            textFormat: Text.PlainText
        }
        Text {
            objectName: "pairingInstructions"
            width: parent.width
            text: translations.pairingInstructions
            color: root.foreground
            font.pixelSize: 18
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
        }
        Row {
            width: parent.width
            height: 185
            spacing: 14
            Image {
                objectName: "pairingQr"
                width: 185
                height: 185
                source: Qt.resolvedUrl("../../assets/icons/connect-pairing-qr.svg")
                sourceSize.width: 185
                sourceSize.height: 185
                smooth: false
                Accessible.name: translations.pairingInstructions
            }
            Column {
                width: parent.width - 199
                spacing: 10
                Text {
                    width: parent.width
                    text: "connect.librescoot.org/pair"
                    color: root.foreground
                    font.pixelSize: 16
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
                Text {
                    objectName: "pairingCode"
                    width: parent.width
                    height: 42
                    text: cloudPairingService.code || "----- -----"
                    color: cloudPairingService.code ? root.foreground : root.muted
                    font.family: "Roboto Condensed"
                    font.pixelSize: 34
                    minimumPixelSize: 20
                    fontSizeMode: Text.Fit
                    textFormat: Text.PlainText
                }
                Text {
                    objectName: "pairingExpiry"
                    width: parent.width
                    text: cloudPairingService.code ? translations.pairingExpires.arg(Math.max(0, Math.ceil(cloudPairingService.expiresAt - root.now))) : ""
                    color: root.muted
                    font.pixelSize: 16
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
                Text {
                    objectName: "pairingStatus"
                    width: parent.width
                    text: root.stateText
                    color: root.foreground
                    font.pixelSize: 18
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
            }
        }
        Text {
            objectName: "pairingSafety"
            width: parent.width
            text: translations.pairingSafety
            color: root.muted
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
        }
    }
    ControlHints {
        objectName: "pairingControls"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        leftHold: translations.controlBack
        rightTap: cloudPairingService.canStop ? translations.pairingStop
            : cloudPairingService.canStart ? translations.pairingStart : ""
        isDark: themeStore.isDark
    }
}

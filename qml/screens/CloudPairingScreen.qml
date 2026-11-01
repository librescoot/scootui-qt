import QtQuick
import ScootUI.Pairing 1.0
import "../widgets/components"

Rectangle {
    id: root
    color: themeStore.isDark ? "black" : "white"
    readonly property color foreground: themeStore.isDark ? "white" : "black"
    readonly property color muted: themeStore.isDark ? "#99ffffff" : "#8a000000"
    readonly property color divider: themeStore.isDark ? "#1fffffff" : "#1f000000"
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
        anchors.fill: parent
        Item {
            width: parent.width
            height: 56
            Text {
                objectName: "pairingTitle"
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: translations.pairingTitle
                color: root.foreground
                font.pixelSize: themeStore.fontBody + 6
                font.weight: Font.DemiBold
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.divider }
        }
        Item {
            width: parent.width
            height: parent.height - 56 - footer.height
            Column {
                id: content
                objectName: "pairingContent"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 12
                Text {
                    objectName: "pairingStatus"
                    width: parent.width
                    text: root.stateText
                    color: root.foreground
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Medium
                    wrapMode: Text.WordWrap
                }
                Text {
                    objectName: "pairingInstructions"
                    width: parent.width
                    visible: cloudPairingService.code !== ""
                    text: visible ? translations.pairingInstructions : ""
                    color: root.foreground
                    font.pixelSize: themeStore.fontBody
                    wrapMode: Text.WordWrap
                }
                Row {
                    width: parent.width
                    height: visible ? 180 : 0
                    visible: cloudPairingService.code !== ""
                    spacing: 14
                    PairingQr {
                        objectName: "pairingQr"
                        width: 180
                        height: 180
                        code: cloudPairingService.code
                        Accessible.name: translations.pairingInstructions
                    }
                    Column {
                        width: parent.width - 194
                        spacing: 10
                        Text {
                            width: parent.width
                            text: "connect.librescoot.org/pair"
                            color: root.foreground
                            font.pixelSize: themeStore.fontCaption
                            wrapMode: Text.WrapAnywhere
                        }
                        Text {
                            objectName: "pairingCode"
                            width: parent.width
                            text: cloudPairingService.code
                            color: root.foreground
                            font.family: "Roboto Condensed"
                            font.pixelSize: 30
                            minimumPixelSize: 20
                            fontSizeMode: Text.Fit
                        }
                        Text {
                            objectName: "pairingExpiry"
                            width: parent.width
                            text: cloudPairingService.code ? translations.pairingExpires.arg(Math.max(0, Math.ceil(cloudPairingService.expiresAt - root.now))) : ""
                            color: root.muted
                            font.pixelSize: themeStore.fontCaption
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Text {
                    objectName: "pairingSafety"
                    width: parent.width
                    text: translations.pairingSafety
                    color: root.muted
                    font.pixelSize: themeStore.fontCaption
                    wrapMode: Text.WordWrap
                }
            }
        }
        Rectangle {
            id: footer
            width: parent.width
            height: controls.height + 1
            color: root.color
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: root.divider }
            ControlHints {
                id: controls
                objectName: "pairingControls"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                reservedRows: 2
                leftHold: translations.controlBack
                rightTap: cloudPairingService.canStop ? translations.pairingStop
                    : cloudPairingService.canStart ? translations.pairingStart : ""
                isDark: themeStore.isDark
            }
        }
    }
}

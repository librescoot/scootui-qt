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
    readonly property bool showCode: cloudPairingService.online && cloudPairingService.running
        && pairingState === "waiting_for_web_setup" && cloudPairingService.code !== ""
        && cloudPairingService.expiresAt > now
    readonly property bool allowStop: cloudPairingService.canStop && pairingState !== "applying"
    readonly property bool checking: cloudPairingService.online && pairingState === "unavailable" && cloudPairingService.loadingStatus
    readonly property bool preparing: cloudPairingService.online && !cloudPairingService.expired
        && (pairingState === "starting" || (pairingState === "waiting_for_web_setup" && cloudPairingService.running && !showCode))
    readonly property string stateText: {
        if (!cloudPairingService.online) return translations.pairingOffline
        if (checking) return translations.pairingChecking
        if (cloudPairingService.expired && pairingState === "waiting_for_web_setup" && cloudPairingService.running) return translations.pairingExpired
        if (preparing) return translations.pairingPreparing
        if (pairingState === "waiting_for_web_setup" && !cloudPairingService.running) return translations.pairingReady
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
        // Resume after this brake signal has finished dispatching.
        Qt.callLater(function() { menuStore.resume() })
    }
    Connections {
        target: inputHandler
        function onLeftHold() { root.close() }
        function onRightTap() {
            if (root.allowStop) cloudPairingService.stop()
            else if (cloudPairingService.canStart) cloudPairingService.start()
            else if (cloudPairingService.online && pairingState === "unavailable") cloudPairingService.refresh()
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
                Rectangle {
                    width: parent.width
                    height: statusText.implicitHeight + 28
                    radius: 10
                    color: themeStore.isDark ? "#202020" : "#f0f0f0"
                    Text {
                        id: statusText
                        objectName: "pairingStatus"
                        anchors.fill: parent
                        anchors.margins: 14
                        text: root.stateText
                        color: root.foreground
                        font.pixelSize: themeStore.fontBody + 2
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                }
                Rectangle {
                    width: 40; height: 40; radius: 20
                    visible: root.preparing || root.checking
                    color: "transparent"
                    border.width: 3
                    border.color: root.muted
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: root.foreground
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: -2
                    }
                    RotationAnimator on rotation { from: 0; to: 360; duration: 900; loops: Animation.Infinite; running: root.preparing || root.checking }
                }
                Text {
                    objectName: "pairingInstructions"
                    width: parent.width
                    visible: root.showCode
                    text: visible ? translations.pairingInstructions : ""
                    color: root.foreground
                    font.pixelSize: themeStore.fontBody
                    wrapMode: Text.WordWrap
                }
                Row {
                    width: parent.width
                    height: visible ? 180 : 0
                    visible: root.showCode
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
                            readonly property int remaining: Math.max(0, Math.ceil(cloudPairingService.expiresAt - root.now))
                            text: root.showCode ? translations.pairingExpires.arg(Math.floor(remaining / 60) + ":" + (remaining % 60).toString().padStart(2, "0")) : ""
                            color: root.muted
                            font.pixelSize: themeStore.fontCaption
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Text {
                    objectName: "pairingSafety"
                    width: parent.width
                    visible: cloudPairingService.online && !root.preparing && !root.showCode
                        && (pairingState === "registered" || pairingState === "not_registered" || (pairingState === "waiting_for_web_setup" && !cloudPairingService.running))
                    text: visible ? translations.pairingSafety : ""
                    color: root.foreground
                    font.pixelSize: themeStore.fontBody
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
                rightTap: root.allowStop ? translations.pairingStop
                    : cloudPairingService.canStart ? translations.pairingStart
                    : cloudPairingService.online && pairingState === "unavailable" && !root.checking ? translations.pairingRetry : ""
                isDark: themeStore.isDark
            }
        }
    }
}

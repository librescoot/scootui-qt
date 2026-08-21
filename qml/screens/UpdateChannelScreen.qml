import QtQuick
import ScootUI 1.0

// Confirmation for a release-channel switch, reached from
// Settings > System > Updates > Switch Release Channel.
//
// What it shows is entirely UpdateChannelService's state: it asks both
// update-service instances what the target channel would fetch and this
// renders the answer. Offline is a state too, not an error, and its copy
// points at Update Mode instead of offering a download nothing could start.
Rectangle {
    id: channelScreen
    color: typeof ThemeStore !== "undefined" && ThemeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof ThemeStore !== "undefined" ? ThemeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)

    // Mirrors UpdateChannelService::State.
    readonly property int stateIdle: 0
    readonly property int stateOffline: 1
    readonly property int stateChecking: 2
    readonly property int stateReady: 3
    readonly property int stateUnavailable: 4
    readonly property int stateFailed: 5

    readonly property int svcState: typeof UpdateChannelService !== "undefined"
                                    ? UpdateChannelService.state : stateIdle
    readonly property string targetChannel: typeof UpdateChannelService !== "undefined"
                                            ? UpdateChannelService.targetChannel : ""
    readonly property string currentChannel: typeof UpdateChannelService !== "undefined"
                                             ? UpdateChannelService.currentChannel : ""
    readonly property string version: typeof UpdateChannelService !== "undefined"
                                      ? UpdateChannelService.version : ""
    readonly property real totalBytes: typeof UpdateChannelService !== "undefined"
                                       ? UpdateChannelService.totalBytes : 0

    // Confirming is only offered where it would actually do something: the
    // channel exists (or we merely failed to price it) and we are online.
    readonly property bool canConfirm: svcState === stateReady || svcState === stateFailed

    function channelLabel(channel) {
        if (typeof Translations === "undefined")
            return channel
        if (channel === "stable")  return Translations.channelStable
        if (channel === "testing") return Translations.channelTesting
        if (channel === "nightly") return Translations.channelNightly
        return channel
    }

    // Binary MB, matching how the download progress elsewhere counts. Rounded
    // to whole units because the copy already says "about".
    function formatBytes(bytes) {
        if (bytes >= 1024 * 1024 * 1024)
            return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
        return Math.round(bytes / (1024 * 1024)) + " MB"
    }

    function bodyText() {
        if (typeof Translations === "undefined")
            return ""
        switch (svcState) {
        case stateChecking:    return Translations.channelSwitchChecking
        case stateReady:       return Translations.channelSwitchSize.arg(formatBytes(totalBytes))
        case stateFailed:      return Translations.channelSwitchSizeUnknown
        case stateUnavailable: return Translations.channelSwitchUnavailable.arg(channelLabel(targetChannel))
        case stateOffline:     return Translations.channelSwitchOffline
        }
        return ""
    }

    function confirmSwitch() {
        if (!canConfirm)
            return
        UpdateChannelService.confirm()
        if (typeof ToastService !== "undefined" && typeof Translations !== "undefined")
            ToastService.showInfo(Translations.channelSwitchDownloadStarted)
    }

    function cancelBack() {
        if (typeof UpdateChannelService !== "undefined")
            UpdateChannelService.cancel()
        if (typeof ScreenStore !== "undefined")
            ScreenStore.closeUpdateChannel()
    }

    // confirm() only changes settings; leaving the screen is this screen's job.
    Connections {
        target: UpdateChannelService
        function onSwitchConfirmed() {
            if (typeof ScreenStore !== "undefined")
                ScreenStore.closeUpdateChannel()
        }
    }

    Connections {
        target: InputHandler
        function onLeftTap()  { channelScreen.cancelBack() }
        function onRightTap() { channelScreen.confirmSwitch() }
    }

    Column {
        anchors.fill: parent

        TopStatusBar {
            id: topBar
            width: parent.width
            height: 40
        }

        Item {
            width: parent.width
            height: parent.height - topBar.height - footer.height

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14

                Text {
                    text: typeof Translations !== "undefined" ? Translations.channelSwitchTitle : ""
                    color: channelScreen.textSecondary
                    font.pixelSize: ThemeStore.fontMicro
                    font.weight: Font.Medium
                    font.letterSpacing: 1.0
                }

                // "Nightly [arrow] Stable". The arrow is a Material Icons glyph
                // rather than U+2192: the Roboto subset shipped on the DBC stops
                // at U+206F, so a text arrow would render as tofu.
                Row {
                    spacing: 10

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: channelScreen.currentChannel !== ""
                        text: channelScreen.channelLabel(channelScreen.currentChannel)
                        color: channelScreen.textSecondary
                        font.pixelSize: ThemeStore.fontHeading
                        font.weight: Font.Bold
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: channelScreen.currentChannel !== ""
                        text: MaterialIcon.iconArrowForward
                        font.family: "Material Icons"
                        font.pixelSize: ThemeStore.fontTitle
                        color: channelScreen.textSecondary
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: channelScreen.channelLabel(channelScreen.targetChannel)
                        color: channelScreen.textPrimary
                        font.pixelSize: ThemeStore.fontHeading
                        font.weight: Font.Bold
                    }
                }

                Text {
                    visible: channelScreen.version !== ""
                    text: channelScreen.version
                    color: channelScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                }

                Text {
                    width: parent.width
                    text: channelScreen.bodyText()
                    color: channelScreen.textPrimary
                    font.pixelSize: ThemeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    wrapMode: Text.WordWrap
                }

                // The way out when there is no connection: the same artifact,
                // carried in by hand.
                Text {
                    width: parent.width
                    visible: channelScreen.svcState === channelScreen.stateOffline
                    text: typeof Translations !== "undefined"
                          ? Translations.channelSwitchOfflineHint.arg(
                                channelScreen.channelLabel(channelScreen.targetChannel))
                          : ""
                    color: channelScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            id: footer
            width: parent.width
            height: controlHints.height + 1
            color: channelScreen.isDark ? "black" : "white"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: channelScreen.dividerColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                leftAction: typeof Translations === "undefined"
                    ? "Back"
                    : (channelScreen.canConfirm ? Translations.controlCancel : Translations.controlBack)
                rightAction: channelScreen.canConfirm && typeof Translations !== "undefined"
                    ? Translations.controlConfirm : ""
            }
        }
    }
}

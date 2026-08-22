import QtQuick
import "../widgets/status_bars"
import "../widgets/components"

// Confirmation for a release-channel switch, reached from
// Settings > System > Updates > Switch Release Channel.
//
// What it shows is entirely UpdateChannelService's state: it asks both
// update-service instances what the target channel would fetch and this
// renders the answer. Offline is a state too, not an error, and its copy
// points at Update Mode instead of offering a download nothing could start.
Rectangle {
    id: channelScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
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

    readonly property int svcState: typeof updateChannelService !== "undefined"
                                    ? updateChannelService.state : stateIdle
    readonly property string targetChannel: typeof updateChannelService !== "undefined"
                                            ? updateChannelService.targetChannel : ""
    readonly property string currentChannel: typeof updateChannelService !== "undefined"
                                             ? updateChannelService.currentChannel : ""
    readonly property string version: typeof updateChannelService !== "undefined"
                                      ? updateChannelService.version : ""
    readonly property real totalBytes: typeof updateChannelService !== "undefined"
                                       ? updateChannelService.totalBytes : 0

    // Confirming is only offered where it would actually do something: the
    // channel exists (or we merely failed to price it) and we are online.
    readonly property bool canConfirm: svcState === stateReady || svcState === stateFailed

    function channelLabel(channel) {
        if (typeof translations === "undefined")
            return channel
        if (channel === "stable")  return translations.channelStable
        if (channel === "testing") return translations.channelTesting
        if (channel === "nightly") return translations.channelNightly
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
        if (typeof translations === "undefined")
            return ""
        switch (svcState) {
        case stateChecking:    return translations.channelSwitchChecking
        case stateReady:       return translations.channelSwitchSize.arg(formatBytes(totalBytes))
        case stateFailed:      return translations.channelSwitchSizeUnknown
        case stateUnavailable: return translations.channelSwitchUnavailable.arg(channelLabel(targetChannel))
        case stateOffline:     return translations.channelSwitchOffline
        }
        return ""
    }

    function confirmSwitch() {
        if (!canConfirm)
            return
        updateChannelService.confirm()
        if (typeof toastService !== "undefined" && typeof translations !== "undefined")
            toastService.showInfo(translations.channelSwitchDownloadStarted)
    }

    function cancelBack() {
        if (typeof updateChannelService !== "undefined")
            updateChannelService.cancel()
        if (typeof screenStore !== "undefined")
            screenStore.closeUpdateChannel()
        if (typeof menuStore !== "undefined")
            menuStore.resume()
    }

    // confirm() only changes settings; leaving the screen is this screen's job.
    Connections {
        target: typeof updateChannelService !== "undefined" ? updateChannelService : null
        function onSwitchConfirmed() {
            if (typeof screenStore !== "undefined")
                screenStore.closeUpdateChannel()
        }
    }

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
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
                    text: typeof translations !== "undefined" ? translations.channelSwitchTitle : ""
                    color: channelScreen.textSecondary
                    font.pixelSize: themeStore.fontMicro
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
                        font.pixelSize: themeStore.fontHeading
                        font.weight: Font.Bold
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: channelScreen.currentChannel !== ""
                        text: MaterialIcon.iconArrowForward
                        font.family: "Material Icons"
                        font.pixelSize: themeStore.fontTitle
                        color: channelScreen.textSecondary
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: channelScreen.channelLabel(channelScreen.targetChannel)
                        color: channelScreen.textPrimary
                        font.pixelSize: themeStore.fontHeading
                        font.weight: Font.Bold
                    }
                }

                Text {
                    visible: channelScreen.version !== ""
                    text: channelScreen.version
                    color: channelScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                }

                Text {
                    width: parent.width
                    text: channelScreen.bodyText()
                    color: channelScreen.textPrimary
                    font.pixelSize: themeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    wrapMode: Text.WordWrap
                }

                // The way out when there is no connection: the same artifact,
                // carried in by hand.
                Text {
                    width: parent.width
                    visible: channelScreen.svcState === channelScreen.stateOffline
                    text: typeof translations !== "undefined"
                          ? translations.channelSwitchOfflineHint.arg(
                                channelScreen.channelLabel(channelScreen.targetChannel))
                          : ""
                    color: channelScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
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
                leftAction: typeof translations === "undefined"
                    ? "Back"
                    : (channelScreen.canConfirm ? translations.controlCancel : translations.controlBack)
                rightAction: channelScreen.canConfirm && typeof translations !== "undefined"
                    ? translations.controlConfirm : ""
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts

Item {
    id: hopNotice
    anchors.fill: parent

    readonly property bool riding: typeof navigationService !== "undefined"
                                   && navigationService.hopPromptVisible
    readonly property bool parked: typeof navigationService !== "undefined"
                                   && navigationService.hopParkedNoticeVisible
    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property var stops: typeof navigationService !== "undefined"
                                 ? navigationService.planStops : []
    readonly property int step: typeof navigationService !== "undefined"
                                ? navigationService.currentStep : 0

    visible: riding || parked

    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 72
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 48, 400)
        height: content.implicitHeight + 24
        radius: themeStore.radiusModal
        color: hopNotice.isDark ? "#E6000000" : "#E6FFFFFF"
        border.width: 1
        border.color: hopNotice.isDark ? "#4DFFFFFF" : "#4D000000"

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: translations.navStopReached.arg(hopNotice.step + 1)
                                                 .arg(hopNotice.stops.length)
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
                color: hopNotice.isDark ? "#FFFFFF" : "#000000"
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: hopNotice.riding
                      ? translations.navAutoContinue.arg(navigationService.hopPromptSecondsRemaining)
                      : translations.navNextUnlock
                font.pixelSize: themeStore.fontBody
                color: hopNotice.riding ? themeStore.statusWarning
                                        : (hopNotice.isDark ? "#FFFFFF" : "#000000")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: hopNotice.riding
                text: translations.navKeepStopHint
                font.pixelSize: themeStore.fontCaption
                color: hopNotice.isDark ? "#B3FFFFFF" : "#B3000000"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}

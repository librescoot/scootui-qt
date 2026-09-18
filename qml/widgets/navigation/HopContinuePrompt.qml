import QtQuick
import QtQuick.Layouts
import "../components"

// Arrival prompt at an intermediate hop. The plan advances on its own when the
// countdown runs out; the rider can confirm early with a right hold or hold
// here with a left hold. Parked/hop-on pauses instead and NavigationService
// stops the countdown.
Item {
    id: hopPrompt
    anchors.fill: parent

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color scrimColor: isDark ? "#000000" : "#FFFFFF"
    readonly property color cardColor: isDark ? "#E6000000" : "#E6FFFFFF"
    readonly property color cardBorder: isDark ? "#4DFFFFFF" : "#4D000000"
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#B3FFFFFF" : "#B3000000"

    readonly property var stops: typeof navigationService !== "undefined"
                                 ? navigationService.planStops : []
    readonly property int step: typeof navigationService !== "undefined"
                                ? navigationService.currentStep : 0
    readonly property string currentLabel: stops.length > step
                                           ? (stops[step].label || "") : ""
    readonly property bool promptVisible: typeof navigationService !== "undefined"
                                          && navigationService.hopPromptVisible

    visible: promptVisible

    Rectangle {
        anchors.fill: parent
        color: hopPrompt.scrimColor
        opacity: 0.35
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        height: content.implicitHeight + 40
        radius: themeStore.radiusModal
        color: hopPrompt.cardColor
        border.width: 1
        border.color: hopPrompt.cardBorder

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: MaterialIcon.iconPlace
                font.family: "Material Icons"
                font.pixelSize: themeStore.fontTitle
                color: themeStore.statusSuccess
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: translations.navStopReached
                      .arg(hopPrompt.step + 1).arg(hopPrompt.stops.length)
                font.pixelSize: themeStore.fontBody
                color: hopPrompt.textSecondary
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                visible: hopPrompt.currentLabel !== ""
                text: hopPrompt.currentLabel
                font.pixelSize: themeStore.fontTitle
                font.weight: Font.Bold
                color: hopPrompt.textPrimary
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: translations.navContinueTo.arg(navigationService.nextStopLabel)
                font.pixelSize: themeStore.fontBody
                color: hopPrompt.textPrimary
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: translations.navAutoContinue
                      .arg(navigationService.hopPromptSecondsRemaining)
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
                color: themeStore.statusWarning
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: translations.navHoldContinue + "   " + translations.navHoldStop
                font.pixelSize: themeStore.fontCaption
                color: hopPrompt.textSecondary
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: hopPrompt.promptVisible
        function onRightHold() {
            if (typeof navigationService !== "undefined")
                navigationService.confirmContinue()
        }
        function onLeftHold() {
            if (typeof navigationService !== "undefined")
                navigationService.declineContinue()
        }
    }
}

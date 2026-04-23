import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/components"

Rectangle {
    id: updateModeScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)

    // Confirm emits ScreenStore::umsModeRequested, which Application.cpp
    // catches and writes to usb:mode — ums-service and the UmsOverlay
    // handle the rest. confirmUpdateMode also closes the info screen so
    // the overlay renders over whatever we came from.
    function confirmEnter() {
        if (typeof screenStore !== "undefined")
            screenStore.confirmUpdateMode()
    }

    function cancelBack() {
        if (typeof screenStore !== "undefined")
            screenStore.closeUpdateModeInfo()
    }

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() { updateModeScreen.cancelBack() }
        function onRightTap() { updateModeScreen.confirmEnter() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        Item { Layout.preferredWidth: 1; Layout.preferredHeight: 12 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: typeof translations !== "undefined"
                  ? translations.updateModeTitle : "Update Mode"
            color: updateModeScreen.textPrimary
            font.pixelSize: themeStore.fontTitle
            font.weight: Font.Bold
        }

        Item { Layout.preferredWidth: 1; Layout.preferredHeight: 12 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.maximumWidth: parent.width - 48
            text: typeof translations !== "undefined"
                  ? translations.updateModeBody1
                  : "Update Mode lets you install updates offline, read logs, and change settings."
            color: updateModeScreen.textPrimary
            font.pixelSize: themeStore.fontBody
            lineHeight: 1.3
            lineHeightMode: Text.ProportionalHeight
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredWidth: 1; Layout.preferredHeight: 8 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.maximumWidth: parent.width - 48
            text: typeof translations !== "undefined"
                  ? translations.updateModeBody2 : ""
            color: updateModeScreen.textSecondary
            font.pixelSize: themeStore.fontCaption
            lineHeight: 1.3
            lineHeightMode: Text.ProportionalHeight
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredWidth: 1; Layout.preferredHeight: 8 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.maximumWidth: parent.width - 48
            text: typeof translations !== "undefined"
                  ? translations.updateModeBody3 : ""
            color: updateModeScreen.textSecondary
            font.pixelSize: themeStore.fontCaption
            lineHeight: 1.3
            lineHeightMode: Text.ProportionalHeight
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true; Layout.maximumHeight: 12 }

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/ScootUI/assets/icons/ums-docs-qr.png"
            sourceSize.width: 110
            sourceSize.height: 110
            width: 110
            height: 110
            visible: status === Image.Ready
        }

        Item { Layout.preferredWidth: 1; Layout.preferredHeight: 4 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: typeof translations !== "undefined"
                  ? translations.updateModeScanHint : "Scan for full instructions"
            color: updateModeScreen.textSecondary
            font.pixelSize: themeStore.fontMicro
        }

        Item { Layout.preferredWidth: 1; Layout.preferredHeight: 8 }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: updateModeScreen.dividerColor
        }

        ControlHints {
            Layout.fillWidth: true
            leftAction: typeof translations !== "undefined"
                        ? translations.controlBack : "Back"
            rightAction: typeof translations !== "undefined"
                         ? translations.updateModeStart : "Start"
        }
    }
}

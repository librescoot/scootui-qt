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

    Column {
        anchors.fill: parent

        TopStatusBar {
            id: topBar
            width: parent.width
            height: 40
        }

        // Scrollable body — Flickable sits between the top bar and the
        // fixed footer so long translations can overflow without shoving
        // the control hints off the bottom of the screen.
        Flickable {
            id: flickable
            width: parent.width
            height: parent.height - topBar.height - footer.height
            contentHeight: bodyColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: bodyColumn
                width: flickable.width
                spacing: 8

                Item { width: 1; height: 12 }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined"
                          ? translations.updateModeTitle : "Update Mode"
                    color: updateModeScreen.textPrimary
                    font.pixelSize: themeStore.fontTitle
                    font.weight: Font.Bold
                }

                Item { width: 1; height: 4 }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: flickable.width - 48
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

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: flickable.width - 48
                    text: typeof translations !== "undefined"
                          ? translations.updateModeBody2 : ""
                    color: updateModeScreen.textSecondary
                    font.pixelSize: themeStore.fontCaption
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: flickable.width - 48
                    text: typeof translations !== "undefined"
                          ? translations.updateModeBody3 : ""
                    color: updateModeScreen.textSecondary
                    font.pixelSize: themeStore.fontCaption
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { width: 1; height: 4 }

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "qrc:/ScootUI/assets/icons/ums-docs-qr.png"
                    sourceSize.width: 110
                    sourceSize.height: 110
                    width: 110
                    height: 110
                    visible: status === Image.Ready
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined"
                          ? translations.updateModeScanHint : "Scan for full instructions"
                    color: updateModeScreen.textSecondary
                    font.pixelSize: themeStore.fontMicro
                }

                Item { width: 1; height: 8 }
            }
        }

        // Non-scrolling footer. Divider + ControlHints are pinned above the
        // bottom edge so the rider always sees the brake bindings.
        Rectangle {
            id: footer
            width: parent.width
            height: controlHints.height + 1
            color: updateModeScreen.isDark ? "black" : "white"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: updateModeScreen.dividerColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                leftAction: typeof translations !== "undefined"
                            ? translations.controlBack : "Back"
                rightAction: typeof translations !== "undefined"
                             ? translations.updateModeStart : "Start"
            }
        }
    }
}

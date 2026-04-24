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

    readonly property bool canScrollDown: flickable.contentHeight > flickable.height
                                           && flickable.contentY + flickable.height < flickable.contentHeight - 2
    readonly property bool canScrollUp: flickable.contentY > 2

    // Left tap scrolls while there's content below, then falls through to
    // Back. Right tap always confirms (Start). Matches NavigationSetup.
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            if (updateModeScreen.canScrollDown) {
                scrollAnim.to = Math.min(flickable.contentY + 100,
                                          flickable.contentHeight - flickable.height)
                scrollAnim.restart()
            } else {
                updateModeScreen.cancelBack()
            }
        }
        function onLeftHold() {
            if (updateModeScreen.canScrollUp) {
                scrollAnim.to = Math.max(flickable.contentY - 100, 0)
                scrollAnim.restart()
            }
        }
        function onRightTap() { updateModeScreen.confirmEnter() }
    }

    Column {
        anchors.fill: parent

        TopStatusBar {
            id: topBar
            width: parent.width
            height: 40
        }

        // Scrollable body — title+body1 share the top row with a QR pinned
        // top-right, body2/body3 flow full-width below. The Flickable sits
        // between the top bar and the fixed footer so long translations
        // never push the control hints off the bottom of the screen.
        Flickable {
            id: flickable
            width: parent.width
            height: parent.height - topBar.height - footer.height
            contentHeight: bodyColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            NumberAnimation {
                id: scrollAnim
                target: flickable
                property: "contentY"
                duration: 200
                easing.type: Easing.OutCubic
            }

            Column {
                id: bodyColumn
                width: flickable.width
                spacing: 12

                Item { width: 1; height: 12 }

                // Top row: title + body1 left, QR + scan hint right
                Item {
                    width: parent.width
                    height: Math.max(topLeft.implicitHeight, qrCol.implicitHeight)

                    Column {
                        id: topLeft
                        anchors.left: parent.left
                        anchors.leftMargin: 24
                        anchors.right: qrCol.left
                        anchors.rightMargin: 16
                        anchors.top: parent.top
                        spacing: 10

                        Text {
                            text: typeof translations !== "undefined"
                                  ? translations.updateModeTitle : "Update Mode"
                            color: updateModeScreen.textPrimary
                            font.pixelSize: themeStore.fontTitle
                            font.weight: Font.Bold
                        }

                        Text {
                            width: parent.width
                            text: typeof translations !== "undefined"
                                  ? translations.updateModeBody1
                                  : "Connect your laptop over USB — the scooter mounts as a drive. Drop updates on, pull logs off."
                            color: updateModeScreen.textPrimary
                            font.pixelSize: themeStore.fontBody
                            lineHeight: 1.3
                            lineHeightMode: Text.ProportionalHeight
                            wrapMode: Text.WordWrap
                        }
                    }

                    Column {
                        id: qrCol
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        spacing: 4

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
                            width: 110
                            horizontalAlignment: Text.AlignHCenter
                            text: typeof translations !== "undefined"
                                  ? translations.updateModeScanHint
                                  : "Scan for full instructions"
                            color: updateModeScreen.textSecondary
                            font.pixelSize: themeStore.fontMicro
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    text: typeof translations !== "undefined"
                          ? translations.updateModeBody2 : ""
                    color: updateModeScreen.textPrimary
                    font.pixelSize: themeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    wrapMode: Text.WordWrap
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    text: typeof translations !== "undefined"
                          ? translations.updateModeBody3 : ""
                    color: updateModeScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    wrapMode: Text.WordWrap
                }

                Item { width: 1; height: 8 }
            }
        }

        // Non-scrolling footer. Divider + ControlHints are pinned above
        // the bottom edge so the rider always sees the brake bindings.
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
                leftAction: updateModeScreen.canScrollDown
                    ? (typeof translations !== "undefined" ? translations.controlScroll : "Scroll")
                    : (typeof translations !== "undefined" ? translations.controlBack : "Back")
                rightAction: typeof translations !== "undefined"
                             ? translations.updateModeStart : "Start"
            }
        }
    }
}

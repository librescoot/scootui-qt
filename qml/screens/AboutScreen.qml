import QtQuick
import QtQuick.Layouts
import "../widgets/components"
import ScootUI

Rectangle {
    id: aboutScreen
    color: ThemeStore.isDark ? "black" : "white"

    readonly property bool isDark: ThemeStore.isDark
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color accentColor: isDark ? "#40C8F0" : "#007A99"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)

    // Warning box colors
    readonly property color warningBg: isDark ? "#1A1200" : "#FFF8E1"
    readonly property color warningBorder: isDark ? "#5C4400" : "#FFB300"
    readonly property color warningText: isDark ? "#FFB300" : "#BF360C"

    readonly property string licenseId: "CC BY-NC-SA 4.0"
    readonly property string websiteUrl: "https://librescoot.org"
    readonly property string copyrightYear: {
        var year = new Date().getFullYear()
        return year > 2025 ? "2025\u2013" + year : "2025"
    }

    // FOSS component data
    readonly property var fossComponents: [
        { name: "Qt 6", license: "LGPL-3.0" },
        { name: "QMapLibre", license: "BSD-2-Clause" },
        { name: "hiredis", license: "BSD-3-Clause" },
        { name: "zlib", license: "Zlib" },
        { name: "OpenStreetMap", license: "ODbL-1.0" },
        { name: "Versatiles", license: "CC0-1.0" },
        { name: "Roboto", license: "Apache-2.0" },
        { name: "Material Icons", license: "Apache-2.0" }
    ]

    Component.onCompleted: {
        SystemInfoService.loadVersions()
    }

    // Easter egg state machine (matches Flutter's _trackEgg step counter)
    // Sequence: down×4, up×3, down×2, up×1 — true=down, false=up
    readonly property var eggSeq: [true, true, true, true, false, false, false, true, true, false]
    property int eggStep: 0

    function trackEgg(isDown) {
        if (isDown === eggSeq[eggStep]) {
            eggStep++
        } else {
            // Reset: if current input matches the first step, start at 1
            eggStep = (isDown === eggSeq[0]) ? 1 : 0
        }
    }

    // Animated scroll target (Flutter: animateTo with 200ms easeOut)
    property real scrollTarget: 0

    function scrollDown() {
        var maxY = Math.max(0, flickable.contentHeight - flickable.height)
        scrollTarget = Math.min(flickable.contentY + 80, maxY)
        scrollAnimation.to = scrollTarget
        scrollAnimation.restart()
        trackEgg(true)
    }

    function scrollUp() {
        scrollTarget = Math.max(flickable.contentY - 80, 0)
        scrollAnimation.to = scrollTarget
        scrollAnimation.restart()
        trackEgg(false)
    }

    function closeScreen() {
        if (eggStep === eggSeq.length) {
            var next = SettingsService.toggleBootAnimation()
            if (next !== "") {
                if (next === "windowsxp")
                    ToastService.showSuccess(Translations.aboutGenuineAdvantage)
                else
                    ToastService.showInfo(Translations.aboutBootThemeRestored)
            }
        }
        ScreenStore.closeAbout()
    }

    // Centralized brake gesture handling via InputHandler
    Connections {
        target: InputHandler
        function onLeftTap() { aboutScreen.scrollDown() }
        function onLeftHold() { aboutScreen.scrollUp() }
        function onRightTap() { aboutScreen.closeScreen() }
    }

    Column {
        anchors.fill: parent

        // ---- Scrollable content (everything scrolls like Flutter) ----
        Flickable {
            id: flickable
            width: parent.width
            height: parent.height - controlBar.height
            contentHeight: scrollContent.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            // Scroll animation (Flutter: animateTo with 200ms easeOut)
            NumberAnimation {
                id: scrollAnimation
                target: flickable
                property: "contentY"
                duration: 200
                easing.type: Easing.OutCubic
            }

            Column {
                id: scrollContent
                width: flickable.width
                spacing: 0

                Item { width: 1; height: 40 }

                // Logo + logotype
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 16

                    Image {
                        source: "qrc:/ScootUI/assets/icons/librescoot-logo.png"
                        width: 64
                        height: 64
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        TintedImage {
                            source: "qrc:/ScootUI/assets/icons/librescoot-logotype.svg"
                            tintColor: aboutScreen.textPrimary
                            width: 180
                            height: 36
                        }

                        Text {
                            text: "sqtui"
                            color: aboutScreen.textSecondary
                            font.pixelSize: ThemeStore.fontBody
                            font.letterSpacing: 1.0
                        }
                    }
                }

                Item { width: 1; height: 8 }

                // FOSS description
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.aboutFossDescription
                    color: aboutScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                    font.italic: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { width: 1; height: 6 }

                // Website URL
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: websiteUrl
                    color: aboutScreen.accentColor
                    font.pixelSize: ThemeStore.fontBody
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { width: 1; height: 12 }

                // Copyright
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: licenseId + "  \u00A9\u00A0" + copyrightYear + " LibreScoot contributors"
                    color: aboutScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                    horizontalAlignment: Text.AlignHCenter
                }

                // Firmware version rows
                Loader {
                    anchors.horizontalCenter: parent.horizontalCenter
                    active: SystemInfoService.versionRows.length > 0
                    sourceComponent: Column {
                        spacing: 0
                        topPadding: 16

                        Repeater {
                            model: SystemInfoService.versionRows

                            Row {
                                spacing: 6
                                anchors.horizontalCenter: parent.horizontalCenter

                                Text {
                                    width: 36
                                    text: modelData.label
                                    color: aboutScreen.textSecondary
                                    font.pixelSize: ThemeStore.fontBody
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignRight
                                    topPadding: 2
                                    bottomPadding: 2
                                }

                                Text {
                                    text: modelData.value
                                    color: aboutScreen.textSecondary
                                    font.pixelSize: ThemeStore.fontBody
                                    font.family: "monospace"
                                    topPadding: 2
                                    bottomPadding: 2
                                }
                            }
                        }
                    }
                }

                Item { width: 1; height: 20 }

                // Divider
                Rectangle {
                    width: parent.width - 80
                    height: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: aboutScreen.dividerColor
                }

                Item { width: 1; height: 8 }

                // Non-commercial warning box
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 80
                    height: warningContent.height + 28
                    color: aboutScreen.warningBg
                    border.color: aboutScreen.warningBorder
                    border.width: 1.5
                    radius: ThemeStore.radiusCard

                    Column {
                        id: warningContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 14
                        spacing: 6

                        // Warning title with icon
                        Row {
                            spacing: 6

                            Text {
                                text: "\ue6cc" // warning_amber
                                font.family: "Material Icons"
                                font.pixelSize: ThemeStore.fontBody
                                color: aboutScreen.warningText
                            }

                            Text {
                                text: Translations.aboutNonCommercialTitle
                                color: aboutScreen.warningText
                                font.pixelSize: ThemeStore.fontBody
                                font.weight: Font.Bold
                                font.letterSpacing: 1.0
                            }
                        }

                        // Commercial prohibited text
                        Text {
                            width: parent.width
                            text: Translations.aboutCommercialProhibited
                            color: aboutScreen.textPrimary
                            font.pixelSize: ThemeStore.fontBody
                            lineHeight: 1.5
                            lineHeightMode: Text.ProportionalHeight
                            wrapMode: Text.WordWrap
                        }

                        Item { width: 1; height: 2 }

                        // Scam warning
                        Text {
                            width: parent.width
                            text: Translations.aboutScamWarning
                            color: aboutScreen.textPrimary
                            font.pixelSize: ThemeStore.fontBody
                            font.weight: Font.DemiBold
                            lineHeight: 1.5
                            lineHeightMode: Text.ProportionalHeight
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Item { width: 1; height: 20 }

                // Divider
                Rectangle {
                    width: parent.width - 80
                    height: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: aboutScreen.dividerColor
                }

                Item { width: 1; height: 8 }

                // FOSS Components header
                Text {
                    x: 40
                    text: Translations.aboutOpenSourceComponents
                    color: aboutScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                    font.weight: Font.Bold
                    font.letterSpacing: 1.5
                }

                Item { width: 1; height: 8 }

                // FOSS component list
                Repeater {
                    model: aboutScreen.fossComponents

                    delegate: Column {
                        width: scrollContent.width
                        spacing: 0

                        Item {
                            width: parent.width
                            height: fossRow.height + 14

                            Row {
                                id: fossRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 40
                                anchors.rightMargin: 40
                                anchors.verticalCenter: parent.verticalCenter

                                Text {
                                    width: parent.width - licenseText.width
                                    text: modelData.name
                                    color: aboutScreen.textPrimary
                                    font.pixelSize: ThemeStore.fontBody
                                }

                                Text {
                                    id: licenseText
                                    text: modelData.license
                                    color: aboutScreen.textSecondary
                                    font.pixelSize: ThemeStore.fontBody
                                    font.family: "monospace"
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width - 80
                            x: 40
                            height: 1
                            color: aboutScreen.dividerColor
                        }
                    }
                }

                Item { width: 1; height: 20 }

                // Divider
                Rectangle {
                    width: parent.width - 80
                    height: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: aboutScreen.dividerColor
                }

                Item { width: 1; height: 8 }

                // Special Thanks header
                Text {
                    x: 40
                    text: Translations.aboutSpecialThanks
                    color: aboutScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                    font.weight: Font.Bold
                    font.letterSpacing: 1.5
                }

                Item { width: 1; height: 8 }

                // Early testers — two-column grid
                Grid {
                    x: 40
                    width: parent.width - 80
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 4

                    Repeater {
                        model: [
                            "alex.aus.berlin", "DSIW",
                            "EndOS", "Fa. Wilhelm Zweiradtechnik",
                            "Felix", "Freal",
                            "inpin", "jaibee1",
                            "Jonas", "Julinho666",
                            "leifbln", "Lena",
                            "ligustah", "suat"
                        ]

                        Text {
                            width: (parent.parent.width - 12) / 2
                            text: modelData
                            color: aboutScreen.textPrimary
                            font.pixelSize: ThemeStore.fontBody
                            elide: Text.ElideRight
                        }
                    }
                }

                Item { width: 1; height: 12 }

                // Patience note (Cin & Tabitha)
                Text {
                    x: 40
                    width: parent.width - 80
                    text: Translations.aboutPatienceNote
                    color: aboutScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                    font.italic: true
                    lineHeight: 1.4
                    lineHeightMode: Text.ProportionalHeight
                    wrapMode: Text.WordWrap
                }

                // Bottom padding
                Item { width: 1; height: 24 }
            }
        }

        // ---- Footer: Control hints (non-scrolling) ----
        Rectangle {
            id: controlBar
            width: parent.width
            height: controlHints.height
            color: aboutScreen.isDark ? "black" : "white"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: aboutScreen.dividerColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                leftAction: Translations.aboutScrollAction
                rightAction: Translations.aboutBackAction
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Rectangle {
    id: aboutScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color accentColor: isDark ? "#40C8F0" : "#0090B8"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)

    // Warning box colors
    readonly property color warningBg: isDark ? "#1A1200" : "#FFF8E1"
    readonly property color warningBorder: isDark ? "#5C4400" : "#FFB300"
    readonly property color warningText: isDark ? "#FFB300" : "#E65100"

    readonly property string licenseId: "CC BY-NC-SA 4.0"
    readonly property string websiteUrl: "https://librescoot.org"
    readonly property string copyrightYear: {
        var year = new Date().getFullYear()
        return year > 2025 ? "2025\u2013" + year : "2025"
    }

    // FOSS component data
    readonly property var fossComponents: [
        { name: "Qt", license: "LGPL-3.0" },
        { name: "QMapLibre", license: "BSD-2-Clause" },
        { name: "hiredis", license: "BSD-3-Clause" },
        { name: "redis-plus-plus", license: "Apache-2.0" }
    ]

    // Easter egg state machine
    // Sequence: [down×4, up×3, down×2, up×1] = [d,d,d,d,u,u,u,d,d,u]
    readonly property var easterEggSequence: ["d","d","d","d","u","u","u","d","d","u"]
    property var easterEggProgress: []
    property bool holdActive: false

    function scrollDown() {
        flickable.contentY = Math.min(
            flickable.contentY + 80,
            flickable.contentHeight - flickable.height
        )
        easterEggProgress.push("d")
        checkEasterEgg()
    }

    function scrollUp() {
        flickable.contentY = Math.max(flickable.contentY - 80, 0)
        easterEggProgress.push("u")
        checkEasterEgg()
    }

    function checkEasterEgg() {
        if (easterEggProgress.length > easterEggSequence.length)
            easterEggProgress = easterEggProgress.slice(-easterEggSequence.length)

        if (easterEggProgress.length === easterEggSequence.length) {
            var match = true
            for (var i = 0; i < easterEggSequence.length; i++) {
                if (easterEggProgress[i] !== easterEggSequence[i]) {
                    match = false
                    break
                }
            }
            if (match) {
                // Check right brake is held
                if (typeof vehicleStore !== "undefined" && vehicleStore.brakeRight === 0) {
                    easterEggProgress = []
                    if (typeof settingsService !== "undefined") {
                        settingsService.togglePlymouthTheme()
                    }
                }
            }
        }
    }

    // Centralized brake gesture handling via InputHandler
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() { aboutScreen.scrollDown() }
        function onLeftHold() { aboutScreen.scrollUp() }
        function onRightTap() {
            if (typeof screenStore !== "undefined") {
                screenStore.setScreen(0)
            }
        }
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

            Column {
                id: scrollContent
                width: flickable.width
                spacing: 0

                Item { width: 1; height: 40 }

                // Logo + title row
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
                        spacing: 2

                        Text {
                            text: "LibreScoot"
                            color: aboutScreen.textPrimary
                            font.pixelSize: 28
                            font.bold: true
                            font.letterSpacing: 0.5
                        }

                        Text {
                            text: "ScootUI"
                            color: aboutScreen.textSecondary
                            font.pixelSize: 14
                            font.letterSpacing: 1.0
                        }
                    }
                }

                Item { width: 1; height: 8 }

                // FOSS description
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.aboutFossDescription
                          : "FOSS firmware for unu Scooter Pro e-mopeds"
                    color: aboutScreen.textSecondary
                    font.pixelSize: 13
                    font.italic: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { width: 1; height: 6 }

                // Website URL
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: websiteUrl
                    color: aboutScreen.accentColor
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { width: 1; height: 12 }

                // Copyright
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: licenseId + "  \u00A9\u00A0" + copyrightYear + " LibreScoot contributors"
                    color: aboutScreen.textSecondary
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
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
                    radius: 8

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
                                font.pixelSize: 14
                                color: aboutScreen.warningText
                            }

                            Text {
                                text: typeof translations !== "undefined"
                                      ? translations.aboutNonCommercialTitle
                                      : "NON-COMMERCIAL SOFTWARE"
                                color: aboutScreen.warningText
                                font.pixelSize: 11
                                font.bold: true
                                font.letterSpacing: 1.0
                            }
                        }

                        // Commercial prohibited text
                        Text {
                            width: parent.width
                            text: typeof translations !== "undefined"
                                  ? translations.aboutCommercialProhibited
                                  : "Commercial distribution, resale, or preinstallation on devices for sale is prohibited under CC BY-NC-SA 4.0."
                            color: aboutScreen.textPrimary
                            font.pixelSize: 12
                            lineHeight: 1.5
                            lineHeightMode: Text.ProportionalHeight
                            wrapMode: Text.WordWrap
                        }

                        Item { width: 1; height: 2 }

                        // Scam warning
                        Text {
                            width: parent.width
                            text: typeof translations !== "undefined"
                                  ? translations.aboutScamWarning
                                  : "If you paid money for this software, or if you purchased a new scooter from a shop or vendor with this software preinstalled, you may have been the victim of a scam. Please report it at https://librescoot.org."
                            color: aboutScreen.textPrimary
                            font.pixelSize: 12
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
                    text: typeof translations !== "undefined"
                          ? translations.aboutOpenSourceComponents
                          : "OPEN SOURCE COMPONENTS"
                    color: aboutScreen.textSecondary
                    font.pixelSize: 11
                    font.bold: true
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
                                    font.pixelSize: 13
                                }

                                Text {
                                    id: licenseText
                                    text: modelData.license
                                    color: aboutScreen.textSecondary
                                    font.pixelSize: 11
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

                // Bottom padding
                Item { width: 1; height: 24 }
            }
        }

        // ---- Footer: Control hints (non-scrolling) ----
        Rectangle {
            id: controlBar
            width: parent.width
            height: controlHints.height + 16
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
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                leftAction: typeof translations !== "undefined"
                            ? translations.aboutScrollAction : "Scroll"
                rightAction: typeof translations !== "undefined"
                             ? translations.aboutBackAction : "Back"
            }
        }
    }
}

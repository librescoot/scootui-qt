import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Rectangle {
    id: aboutScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
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

    // Authorized installation partners
    readonly property var authorizedPartners: [
        { name: "Fa. Wilhelm Zweiradtechnik", lines: ["General-Pape-Str. 8", "12101 Berlin"] }
    ]

    Component.onCompleted: {
        if (typeof systemInfoService !== "undefined")
            systemInfoService.loadVersions()
    }

    // Easter egg state machines. true=down, false=up.
    // Boot animation: down x4, up x3, down x2, up x1
    readonly property var eggSeq: [true, true, true, true, false, false, false, true, true, false]
    property int eggStep: 0

    // Milestone easter eggs: mirror sequence, down x1, up x2, down x3, up x4
    readonly property var milestoneEggSeq: [true, false, false, true, true, true, false, false, false, false]
    property int milestoneEggStep: 0

    function trackEgg(isDown) {
        if (isDown === eggSeq[eggStep]) {
            eggStep++
        } else {
            eggStep = (isDown === eggSeq[0]) ? 1 : 0
        }
        if (isDown === milestoneEggSeq[milestoneEggStep]) {
            milestoneEggStep++
        } else {
            milestoneEggStep = (isDown === milestoneEggSeq[0]) ? 1 : 0
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
            if (typeof settingsService !== "undefined") {
                var next = settingsService.toggleBootAnimation()
                if (typeof toastService !== "undefined" && next !== "") {
                    if (next === "windowsxp")
                        toastService.showSuccess(typeof translations !== "undefined"
                            ? translations.aboutGenuineAdvantage : "Genuine Advantage activated.")
                    else
                        toastService.showInfo(typeof translations !== "undefined"
                            ? translations.aboutBootThemeRestored : "Boot theme: LibreScoot restored.")
                }
            }
        }
        if (milestoneEggStep === milestoneEggSeq.length) {
            if (typeof odometerMilestoneService !== "undefined") {
                var enabled = !odometerMilestoneService.easterEggsEnabled
                odometerMilestoneService.easterEggsEnabled = enabled
                if (typeof toastService !== "undefined") {
                    toastService.showInfo(enabled
                        ? "Milestone easter eggs unlocked"
                        : "Milestone easter eggs locked")
                }
            }
        }
        if (typeof screenStore !== "undefined") {
            screenStore.closeAbout()
        }
    }

    // Centralized brake gesture handling via InputHandler
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
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

                // Header is always black, even in light mode, so the logo's
                // baked-in flame and white wordmark stay readable.
                Rectangle {
                    width: parent.width
                    height: 130
                    color: "black"

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/ScootUI/assets/icons/librescoot-logo-small.png"
                        width: 240
                        height: 70
                    }
                }

                Item { width: 1; height: 16 }

                // FOSS description
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.aboutFossDescription
                          : "FOSS firmware for unu Scooter Pro e-mopeds"
                    color: aboutScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    font.italic: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { width: 1; height: 6 }

                // Website URL
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: websiteUrl
                    color: aboutScreen.accentColor
                    font.pixelSize: themeStore.fontBody
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { width: 1; height: 12 }

                // Copyright
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: licenseId + "  \u00A9\u00A0" + copyrightYear + " LibreScoot contributors"
                    color: aboutScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    horizontalAlignment: Text.AlignHCenter
                }

                // Firmware version rows
                Loader {
                    anchors.horizontalCenter: parent.horizontalCenter
                    active: typeof systemInfoService !== "undefined" &&
                            systemInfoService.versionRows.length > 0
                    sourceComponent: Column {
                        spacing: 0
                        topPadding: 16

                        Repeater {
                            model: typeof systemInfoService !== "undefined"
                                   ? systemInfoService.versionRows : []

                            Row {
                                spacing: 6
                                anchors.horizontalCenter: parent.horizontalCenter

                                Text {
                                    width: 36
                                    text: modelData.label
                                    color: aboutScreen.textSecondary
                                    font.pixelSize: themeStore.fontBody
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignRight
                                    topPadding: 2
                                    bottomPadding: 2
                                }

                                Text {
                                    text: modelData.value
                                    color: aboutScreen.textSecondary
                                    font.pixelSize: themeStore.fontBody
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
                    radius: themeStore.radiusCard

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
                                text: MaterialIcon.iconWarningAmber
                                font.family: "Material Icons"
                                font.pixelSize: themeStore.fontBody
                                color: aboutScreen.warningText
                            }

                            Text {
                                text: typeof translations !== "undefined"
                                      ? translations.aboutNonCommercialTitle
                                      : "NON-COMMERCIAL SOFTWARE"
                                color: aboutScreen.warningText
                                font.pixelSize: themeStore.fontBody
                                font.weight: Font.Bold
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
                            font.pixelSize: themeStore.fontBody
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
                            font.pixelSize: themeStore.fontBody
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
                    font.pixelSize: themeStore.fontBody
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
                                    font.pixelSize: themeStore.fontBody
                                }

                                Text {
                                    id: licenseText
                                    text: modelData.license
                                    color: aboutScreen.textSecondary
                                    font.pixelSize: themeStore.fontBody
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
                    text: typeof translations !== "undefined"
                          ? translations.aboutSpecialThanks
                          : "SPECIAL THANKS TO THE EARLY TESTERS"
                    color: aboutScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
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
                            "alex.aus.berlin", "Bendix",
                            "DSIW", "EndOS",
                            "Fa. Wilhelm Zweiradtechnik", "Felix",
                            "Freal", "inpin",
                            "jaibee1", "Jonas",
                            "Julinho666", "leifbln",
                            "Lena", "ligustah",
                            "suat"
                        ]

                        Text {
                            width: (parent.parent.width - 12) / 2
                            text: modelData
                            color: aboutScreen.textPrimary
                            font.pixelSize: themeStore.fontBody
                            elide: Text.ElideRight
                        }
                    }
                }

                Item { width: 1; height: 12 }

                // Patience note (Cin & Tabitha)
                Text {
                    x: 40
                    width: parent.width - 80
                    text: typeof translations !== "undefined"
                          ? translations.aboutPatienceNote
                          : "And Cin and Tabitha for their patience with the scooters in the hallway."
                    color: aboutScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    font.italic: true
                    lineHeight: 1.4
                    lineHeightMode: Text.ProportionalHeight
                    wrapMode: Text.WordWrap
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

                // Authorized installation partners header
                Text {
                    x: 40
                    text: typeof translations !== "undefined"
                          ? translations.aboutAuthorizedPartners
                          : "AUTHORIZED INSTALLATION PARTNERS"
                    color: aboutScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Bold
                    font.letterSpacing: 1.5
                }

                Item { width: 1; height: 8 }

                // Authorized partners list
                Repeater {
                    model: aboutScreen.authorizedPartners

                    delegate: Column {
                        x: 40
                        width: scrollContent.width - 80
                        spacing: 0
                        bottomPadding: 10

                        Text {
                            width: parent.width
                            text: modelData.name
                            color: aboutScreen.textPrimary
                            font.pixelSize: themeStore.fontBody
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: modelData.lines

                            Text {
                                width: parent.parent.width
                                text: modelData
                                color: aboutScreen.textSecondary
                                font.pixelSize: themeStore.fontBody
                                lineHeight: 1.3
                                lineHeightMode: Text.ProportionalHeight
                                wrapMode: Text.WordWrap
                            }
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
                leftAction: typeof translations !== "undefined"
                            ? translations.aboutScrollAction : "Scroll"
                rightAction: typeof translations !== "undefined"
                             ? translations.aboutBackAction : "Back"
            }
        }
    }
}

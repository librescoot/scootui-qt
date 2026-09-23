import QtQuick
import "../widgets/status_bars"
import "../widgets/components"

Rectangle {
    id: screen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)
    readonly property bool learning: typeof keycardStore !== "undefined" && keycardStore.learning
    readonly property bool teaching: typeof keycardStore !== "undefined" && keycardStore.masterTeachIn
    readonly property bool bootstrap: typeof keycardStore !== "undefined" && keycardStore.masterBootstrap
    readonly property bool active: learning || teaching || bootstrap
    property string flow: bootstrap ? "bootstrap" : teaching ? "master" : "unlock"

    function captureFlow() {
        if (bootstrap) flow = "bootstrap"
        else if (teaching) flow = "master"
        else if (learning) flow = "unlock"
    }

    function titleText() {
        if (flow === "bootstrap") return translations.keycardSetupTitle
        if (flow === "master") return translations.keycardAddMaster
        return translations.keycardEnrollTitle
    }

    function bodyText() {
        if (flow === "bootstrap") return translations.keycardSetupBody
        if (flow === "master") return translations.keycardMasterTeachInBody
        if (learning) return translations.keycardEnrollActiveBody
        return translations.keycardEnrollInfoBody
    }

    function feedbackText() {
        if (typeof keycardStore === "undefined") return ""
        if (keycardStore.lastScannedKind === "phone") {
            switch (keycardStore.scanStatus) {
            case "accepted": return translations.keycardPhoneAccepted
            case "duplicate": return translations.keycardPhoneDuplicate
            case "rejected": return translations.keycardPhoneRejected
            case "error": return translations.keycardPhoneError
            }
        }
        switch (keycardStore.scanStatus) {
        case "accepted": return flow === "unlock" ? translations.keycardScanAccepted
                                                   : translations.keycardMasterSaved
        case "duplicate": return translations.keycardScanDuplicate
        case "rejected": return translations.keycardScanRejected
        case "error": return translations.keycardScanError
        }
        return ""
    }

    function back() {
        if (typeof screenStore !== "undefined") screenStore.closeKeycardEnrollInfo()
        if (typeof menuStore !== "undefined") menuStore.resume()
    }

    function action() {
        if (typeof keycardStore === "undefined") return
        if (!active && flow !== "unlock") return
        if (bootstrap) keycardStore.skipMasterBootstrap()
        else if (teaching) keycardStore.stopMasterEnroll()
        else if (learning) keycardStore.stopEnroll()
        else keycardStore.startEnroll()
    }

    readonly property bool canScrollDown: flickable.contentHeight > flickable.height
                                           && flickable.contentY + flickable.height < flickable.contentHeight - 2
    readonly property bool canScrollUp: flickable.contentY > 2

    Component.onCompleted: captureFlow()

    Connections {
        target: typeof keycardStore !== "undefined" ? keycardStore : null
        function onLearnStateChanged() { screen.captureFlow() }
    }

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            if (!screen.canScrollDown) return
            scrollAnim.to = Math.min(flickable.contentY + 100,
                                     flickable.contentHeight - flickable.height)
            scrollAnim.restart()
        }
        function onLeftHold() { screen.back() }
        function onRightHold() {
            if (!screen.canScrollUp) return
            scrollAnim.to = Math.max(flickable.contentY - 100, 0)
            scrollAnim.restart()
        }
        function onRightTap() { screen.action() }
    }

    Column {
        anchors.fill: parent

        TopStatusBar {
            id: topBar
            width: parent.width
            height: 40
        }

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
                spacing: 14

                Item { width: 1; height: 16 }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    text: screen.titleText()
                    color: screen.textPrimary
                    font.pixelSize: themeStore.fontTitle
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    text: screen.bodyText()
                    color: screen.textPrimary
                    font.pixelSize: themeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    visible: screen.learning
                    text: translations.keycardEnrollDetail
                    color: screen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    height: 1
                    visible: screen.learning || keycardStore.lastScannedUid !== ""
                    color: screen.dividerColor
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    visible: screen.learning
                    text: translations.keycardDetectedCount.arg(keycardStore.sessionCardCount)
                                                       .arg(keycardStore.sessionPhoneCount)
                    color: screen.textPrimary
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    visible: keycardStore.lastScannedUid !== ""
                    text: keycardStore.lastScannedKind === "phone"
                          ? translations.keycardLastPhone.arg(keycardStore.lastScannedUid.slice(-8))
                          : translations.keycardLastCard.arg(keycardStore.lastScannedUid)
                    color: screen.textPrimary
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WrapAnywhere
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    visible: text !== ""
                    text: screen.feedbackText()
                    color: keycardStore.scanStatus === "error" || keycardStore.scanStatus === "rejected"
                           ? themeStore.statusWarning : screen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { width: 1; height: 8 }
            }
        }

        Rectangle {
            id: footer
            width: parent.width
            height: controlHints.height + 1
            color: screen.isDark ? "black" : "white"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: screen.dividerColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                reservedRows: 2
                leftTap: screen.canScrollDown ? translations.controlScroll : ""
                leftHold: translations.controlBack
                rightHold: screen.canScrollUp ? translations.controlScrollUp : ""
                rightTap: screen.bootstrap ? translations.keycardSkip
                          : screen.active ? translations.keycardDone
                          : screen.flow === "unlock" ? translations.keycardEnrollStart : ""
            }
        }
    }
}

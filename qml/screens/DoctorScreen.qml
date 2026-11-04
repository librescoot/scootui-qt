import QtQuick
import "../widgets/components"

Rectangle {
    id: root
    objectName: "doctorScreen"
    readonly property bool dark: themeStore.isDark
    readonly property color foreground: dark ? "white" : "black"
    readonly property color secondary: dark ? "#B3FFFFFF" : "#99000000"
    readonly property color divider: dark ? "#33FFFFFF" : "#33000000"
    color: dark ? "black" : "white"

    function verdictText(verdict) {
        switch (verdict) {
        case "ok": return translations.doctorOK
        case "attention": return translations.doctorAttention
        case "warning": return translations.doctorWarning
        default: return translations.doctorUnknown
        }
    }
    function verdictColor(verdict) {
        switch (verdict) {
        case "ok": return dark ? "#66BB6A" : "#23752A"
        case "attention": return dark ? "#FF5252" : "#B00020"
        case "warning": return dark ? "#FFB300" : "#925000"
        default: return secondary
        }
    }
    function checkName(name) {
        switch (name) {
        case "services": return translations.doctorServices
        case "link": return translations.doctorLink
        case "kernel": return translations.doctorKernel
        case "ota": return translations.menuUpdates
        case "faults": return translations.menuFaults
        case "storage": return translations.doctorStorage
        default: return name
        }
    }
    function errorText(error) {
        switch (error) {
        case "unavailable": return translations.doctorUnavailable
        case "timeout": return translations.doctorTimeout
        case "unsupported": return translations.doctorUnsupported
        case "cancelled": return translations.doctorCancelled
        default: return translations.doctorInvalid
        }
    }
    function summary(board) {
        if (board.state === "running") return translations.doctorRunning
        if (board.state === "idle") return translations.doctorIdle
        if (board.state === "error") return translations.doctorUnknown
        if (board.attention + board.warnings + board.unknown === 0) return translations.doctorOK
        return translations.doctorSummary.arg(board.attention).arg(board.warnings).arg(board.unknown)
    }
    function closeScreen() {
        screenStore.closeDoctor()
        menuStore.resume()
    }
    function retry() {
        if (!visible || doctorService.running) return
        scroll.contentY = 0
        doctorService.start()
    }
    function scrollBy(amount) {
        scroll.contentY = Math.max(0, Math.min(scroll.contentY + amount, scroll.contentHeight - scroll.height))
    }
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: root.visible
        function onLeftTap() { root.scrollBy(120) }
        function onRightHold() { root.scrollBy(-120) }
        function onLeftHold() { root.closeScreen() }
        function onRightTap() { root.retry() }
    }
    onVisibleChanged: if (visible) scroll.contentY = 0

    Column {
        anchors.fill: parent
        Item {
            width: parent.width
            height: 52
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: translations.doctorTitle
                color: root.foreground
                font.pixelSize: themeStore.fontHeading
                font.bold: true
            }
        }
        Row {
            id: summaries
            width: parent.width
            height: 78
            Repeater {
                model: doctorService.boards
                delegate: Rectangle {
                    required property var modelData
                    width: summaries.width / 2
                    height: summaries.height
                    color: "transparent"
                    border.color: root.divider
                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 3
                        Text {
                            text: modelData.id
                            color: root.foreground
                            font.bold: true
                            font.pixelSize: themeStore.fontBody
                        }
                        Text {
                            objectName: "doctorSummary_" + modelData.id
                            width: parent.width
                            text: root.summary(modelData)
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            color: root.verdictColor(modelData.state !== "done" ? "unknown"
                                : modelData.attention ? "attention" : modelData.warnings ? "warning"
                                : modelData.unknown ? "unknown" : "ok")
                            font.pixelSize: themeStore.fontCaption
                        }
                    }
                }
            }
        }
        Flickable {
            id: scroll
            objectName: "doctorScroll"
            width: parent.width
            height: root.height - 52 - summaries.height - hints.height
            contentHeight: details.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: details
                width: scroll.width
                Repeater {
                    model: doctorService.boards
                    delegate: Column {
                        id: boardSection
                        required property var modelData
                        width: details.width
                        Item {
                            width: parent.width
                            height: 40
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 20
                                anchors.verticalCenter: parent.verticalCenter
                                text: boardSection.modelData.id
                                font.pixelSize: themeStore.fontTitle
                                font.bold: true
                                color: root.foreground
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 20
                                anchors.verticalCenter: parent.verticalCenter
                                text: boardSection.modelData.completedAt
                                      ? Qt.formatTime(new Date(boardSection.modelData.completedAt), "HH:mm:ss") : ""
                                color: root.secondary
                                font.pixelSize: themeStore.fontCaption
                            }
                        }
                        Text {
                            width: parent.width - 40
                            x: 20
                            visible: boardSection.modelData.state !== "done"
                            text: boardSection.modelData.state === "error"
                                ? root.errorText(boardSection.modelData.error) : root.summary(boardSection.modelData)
                            color: root.secondary
                            wrapMode: Text.WordWrap
                            textFormat: Text.PlainText
                            font.pixelSize: themeStore.fontBody
                            bottomPadding: 16
                        }
                        Repeater {
                            model: boardSection.modelData.checks
                            delegate: Column {
                                id: check
                                required property var modelData
                                width: boardSection.width - 40
                                x: 20
                                spacing: 6
                                Row {
                                    spacing: 8
                                    Text {
                                        text: root.verdictText(check.modelData.verdict)
                                        color: root.verdictColor(check.modelData.verdict)
                                        font.pixelSize: themeStore.fontBody
                                        font.bold: true
                                    }
                                    Text {
                                        text: root.checkName(check.modelData.name)
                                        color: root.foreground
                                        font.pixelSize: themeStore.fontBody
                                    }
                                }
                                Text {
                                    objectName: "doctorDetail_" + boardSection.modelData.id + "_" + check.modelData.name
                                    width: parent.width
                                    text: check.modelData.detail
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                    color: root.foreground
                                    font.pixelSize: themeStore.fontBody
                                    visible: text !== ""
                                }
                                Text {
                                    width: parent.width
                                    text: check.modelData.hint
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                    color: root.secondary
                                    font.pixelSize: themeStore.fontCaption
                                    visible: text !== "" && check.modelData.verdict !== "ok"
                                }
                                Item { width: 1; height: 8 }
                                Rectangle { width: parent.width; height: 1; color: root.divider }
                                Item { width: 1; height: 8 }
                            }
                        }
                    }
                }
            }
        }
        ControlHints {
            id: hints
            width: parent.width
            reservedRows: 2
            leftTap: scroll.contentY + scroll.height < scroll.contentHeight - 2 ? translations.controlScroll : ""
            leftHold: translations.controlBack
            rightTap: doctorService.running ? "" : translations.doctorRetry
            rightHold: scroll.contentY > 2 ? translations.controlScrollUp : ""
        }
    }
}

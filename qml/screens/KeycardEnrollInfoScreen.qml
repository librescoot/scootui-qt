import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/components"

Rectangle {
    id: screen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"
    readonly property bool dark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property bool learning: typeof keycardStore !== "undefined" && keycardStore.learning
    readonly property bool teaching: typeof keycardStore !== "undefined" && keycardStore.masterTeachIn
    readonly property bool bootstrap: typeof keycardStore !== "undefined" && keycardStore.masterBootstrap
    function back() {
        if (typeof screenStore !== "undefined") screenStore.closeKeycardEnrollInfo()
        if (typeof menuStore !== "undefined") menuStore.resume()
    }
    function action() {
        if (typeof keycardStore === "undefined") return
        if (bootstrap) keycardStore.skipMasterBootstrap()
        else if (teaching) keycardStore.stopMasterEnroll()
        else if (learning) keycardStore.stopEnroll()
        else keycardStore.startEnroll()
    }
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftHold() { screen.back() }
        function onRightTap() { screen.action() }
    }
    ColumnLayout {
        anchors.fill: parent
        TopStatusBar { Layout.fillWidth: true; Layout.preferredHeight: 40 }
        Item { Layout.fillHeight: true }
        Text { Layout.fillWidth: true; text: bootstrap ? translations.keycardSetupTitle : teaching ? translations.keycardAddMaster : translations.keycardEnrollTitle; horizontalAlignment: Text.AlignHCenter; color: screen.dark ? "white" : "black"; font.pixelSize: 26; font.bold: true }
        Text { Layout.fillWidth: true; Layout.leftMargin: 36; Layout.rightMargin: 36; text: bootstrap ? translations.keycardSetupBody : teaching ? translations.keycardMasterTeachInBody : learning ? translations.keycardEnrollActiveBody : translations.keycardEnrollInfoBody; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: screen.dark ? "#cccccc" : "#444444"; font.pixelSize: 17 }
        Item { Layout.fillHeight: true }
        ControlHints { Layout.fillWidth: true; leftHold: typeof translations !== "undefined" ? translations.controlBack : "Back"; rightTap: bootstrap ? translations.keycardSkip : teaching ? translations.keycardDone : learning ? translations.keycardDone : translations.keycardEnrollStart }
    }
}

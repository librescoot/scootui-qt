import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/components"

Rectangle {
    id: screen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"
    readonly property bool dark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    property int selected: 0
    property string confirmUid: ""
    readonly property var rows: {
        var result = [{ label: translations.keycardEnrollTitle, kind: "add" }, { label: translations.keycardAddMaster, kind: "master-add" }]
        if (typeof keycardStore !== "undefined") {
            for (var i = 0; i < keycardStore.unlockCards.length; ++i) result.push({label: keycardStore.unlockCards[i], kind: "unlock"})
            for (var j = 0; j < keycardStore.masterCards.length; ++j) result.push({label: keycardStore.masterCards[j], kind: "master"})
        }
        return result
    }
    function resetConfirmation() { confirmUid = "" }
    function move(delta) { if (!rows.length) return; selected = (selected + delta + rows.length) % rows.length; resetConfirmation(); list.positionViewAtIndex(selected, ListView.Contain) }
    function activate() {
        if (typeof keycardStore === "undefined" || !rows.length) return
        var row = rows[selected]
        if (row.kind === "add") { keycardStore.startEnroll(); if (typeof screenStore !== "undefined") screenStore.showKeycardEnrollInfo(); return }
        if (row.kind === "master-add") { keycardStore.startMasterEnroll(); if (typeof screenStore !== "undefined") screenStore.showKeycardEnrollInfo(); return }
        if (row.kind === "master") { keycardStore.removeMaster(row.label); resetConfirmation(); return }
        if (keycardStore.unlockCardCount > 1) { keycardStore.removeCard(row.label); resetConfirmation(); return }
        if (confirmUid === row.label) { keycardStore.removeCardForced(row.label); resetConfirmation() }
        else confirmUid = row.label
    }
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() { screen.move(1) }
        function onRightHold() { screen.move(-1) }
        function onLeftHold() { if (typeof screenStore !== "undefined") screenStore.closeKeycardManage(); if (typeof menuStore !== "undefined") menuStore.resume() }
        function onRightTap() { screen.activate() }
    }
    Connections { target: typeof keycardStore !== "undefined" ? keycardStore : null; function onUnlockCardsChanged() { screen.resetConfirmation(); if (screen.selected >= screen.rows.length) screen.selected = Math.max(0, screen.rows.length - 1) } function onMasterCardsChanged() { screen.resetConfirmation(); if (screen.selected >= screen.rows.length) screen.selected = Math.max(0, screen.rows.length - 1) } function onLearnStateChanged() { screen.resetConfirmation() } }
    ColumnLayout {
        anchors.fill: parent
        TopStatusBar { Layout.fillWidth: true; Layout.preferredHeight: 40 }
        Text { Layout.fillWidth: true; text: translations.menuKeycards; horizontalAlignment: Text.AlignHCenter; color: screen.dark ? "white" : "black"; font.pixelSize: 24; font.bold: true }
        ListView { id: list; Layout.fillWidth: true; Layout.fillHeight: true; model: screen.rows; clip: true; delegate: Rectangle { required property var modelData; required property int index; width: list.width; height: 42; color: index === screen.selected ? (screen.dark ? "#345" : "#cdf") : "transparent"; Text { anchors.centerIn: parent; text: modelData.label + (modelData.kind === "unlock" && screen.confirmUid === modelData.label ? " - " + translations.keycardConfirmRemoveLast : ""); color: screen.dark ? "white" : "black"; font.pixelSize: 17 } } }
        ControlHints { Layout.fillWidth: true; leftTap: "Next"; leftHold: "Back"; rightTap: "Select"; rightHold: "Previous" }
    }
}

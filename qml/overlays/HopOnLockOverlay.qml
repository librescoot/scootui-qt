import QtQuick

// Full-screen lock overlay shown while hop-on / hop-off mode is engaged.
// The display backlight is OFF underneath this overlay (set by HopOnStore
// via DashboardStore::setBacklightOff(true)), but the QML scene is still
// alive and watches for the user-defined unlock combo. As soon as the
// matcher accepts the sequence the overlay disappears and the backlight
// comes back on.
//
// Intentionally does NOT visualize the unlock sequence as the user enters
// it — that would defeat the secret combo.
Item {
    id: lockOverlay
    anchors.fill: parent

    // HopOnStore.Mode.Locked == 2
    readonly property int modeLocked: 2

    property int mode: typeof hopOnStore !== "undefined" ? hopOnStore.mode : 0
    // Experimental gate: never render unless `experimental.hop-on` is set.
    property bool gateOpen: typeof settingsStore !== "undefined" && settingsStore.experimentalHopOn

    visible: gateOpen && mode === modeLocked

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color scrimColor:    isDark ? "#000000" : "#FFFFFF"
    readonly property color cardColor:     isDark ? "#CC000000" : "#CCFFFFFF"
    readonly property color cardBorder:    isDark ? "#4DFFFFFF" : "#4D000000"
    readonly property color textPrimary:   isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#B3FFFFFF" : "#B3000000"

    Rectangle {
        anchors.fill: parent
        // Solid black scrim — backlight is off anyway, but if the dashboard
        // happens to be running with the backlight on (during dev/debug)
        // we still want a clearly distinct lock screen.
        color: lockOverlay.scrimColor
        opacity: 0.95
    }

    Column {
        anchors.centerIn: parent
        spacing: 0

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.parent.width - 48, 460)
            height: cardContent.height + 48
            color: lockOverlay.cardColor
            border.width: 1
            border.color: lockOverlay.cardBorder
            radius: typeof themeStore !== "undefined" ? themeStore.radiusModal : 16

            Column {
                id: cardContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 16

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\ue3ae" // material lock
                    font.family: "Material Icons"
                    font.pixelSize: typeof themeStore !== "undefined" ? themeStore.fontHero : 48
                    color: lockOverlay.textPrimary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.hopOnLockedTitle : "Hop-on active"
                    font.pixelSize: typeof themeStore !== "undefined" ? themeStore.fontHeading : 28
                    font.weight: Font.Bold
                    color: lockOverlay.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.hopOnLockedHint : "Press your combo to unlock"
                    font.pixelSize: typeof themeStore !== "undefined" ? themeStore.fontBody : 18
                    color: lockOverlay.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }
        }
    }
}

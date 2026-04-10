import QtQuick

Item {
    id: autoLockOverlay
    anchors.fill: parent

    // VehicleState.Parked = 4 (see src/models/Enums.h: Unknown, StandBy, ReadyToDrive, Off, Parked)
    readonly property int stateParked: 4

    property int vehicleState: typeof vehicleStore !== "undefined" ? vehicleStore.state : 0
    property int remaining: typeof autoStandbyStore !== "undefined" ? autoStandbyStore.remainingSeconds : 0

    // Theme-aware colors. Orange countdown stays as-is (accent works in both modes).
    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color scrimColor:    isDark ? "#000000" : "#FFFFFF"
    readonly property color cardColor:     isDark ? "#CC000000" : "#CCFFFFFF"
    readonly property color cardBorder:    isDark ? "#4DFFFFFF" : "#4D000000"
    readonly property color textPrimary:   isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#B3FFFFFF" : "#B3000000"

    // Show only during the last 60s of the auto-lock idle timer, while parked.
    // The countdown is interrupted automatically: any user input (brake, kickstand,
    // seatbox button) makes vehicle-service reset the timer and republish a later
    // deadline, so `remaining` jumps back above 60 and this overlay disappears.
    visible: vehicleState === stateParked && remaining > 0 && remaining <= 60

    Rectangle {
        anchors.fill: parent
        color: autoLockOverlay.scrimColor
        opacity: 0.9
    }

    Column {
        anchors.centerIn: parent
        spacing: 0

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.parent.width - 48, 420)
            height: cardContent.height + 48
            color: autoLockOverlay.cardColor
            border.width: 1
            border.color: autoLockOverlay.cardBorder
            radius: themeStore.radiusModal

            Column {
                id: cardContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 16

                // Lock icon
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: MaterialIcon.iconLock
                    font.family: "Material Icons"
                    font.pixelSize: themeStore.fontHero
                    color: autoLockOverlay.textPrimary
                }

                // Title
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.autoLockTitle : "Auto-Locking"
                    font.pixelSize: themeStore.fontHeading
                    font.weight: Font.Bold
                    color: autoLockOverlay.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                // Big countdown number
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: autoLockOverlay.remaining + "s"
                    font.pixelSize: themeStore.fontHero
                    font.weight: Font.Bold
                    color: "#FF9800"
                }

                // Cancel hint
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined"
                          ? translations.autoLockCancelHint
                          : "Touch a brake or kickstand to cancel"
                    font.pixelSize: themeStore.fontBody
                    color: autoLockOverlay.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }
        }
    }
}

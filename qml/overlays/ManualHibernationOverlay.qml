import QtQuick
import ScootUI 1.0

Item {
    id: hibernationOverlay
    anchors.fill: parent

    // VehicleState enum values from Enums.h
    readonly property int stateHibernating: 7
    readonly property int stateHibernatingImminent: 8
    readonly property int stateWaitingHibernation: 13
    readonly property int stateWaitingHibernationAdvanced: 14
    readonly property int stateWaitingHibernationSeatbox: 15
    readonly property int stateWaitingHibernationConfirm: 16

    property int vehicleState: VehicleStore.state
    property bool bothBrakesHeld: true
                                  ? (VehicleStore.brakeLeft === 1 && VehicleStore.brakeRight === 1)
                                  : false

    property bool isHibernating: vehicleState === stateWaitingHibernation
                                 || vehicleState === stateWaitingHibernationAdvanced
                                 || vehicleState === stateWaitingHibernationSeatbox
                                 || vehicleState === stateWaitingHibernationConfirm
                                 || vehicleState === stateHibernating
                                 || vehicleState === stateHibernatingImminent

    property bool isPromptMode: vehicleState === stateWaitingHibernation
                                || vehicleState === stateWaitingHibernationAdvanced
    property bool isSeatboxMode: vehicleState === stateWaitingHibernationSeatbox
    property bool isConfirmMode: vehicleState === stateWaitingHibernationConfirm
                                 || vehicleState === stateHibernating
                                 || vehicleState === stateHibernatingImminent

    // Theme-aware colors. Accent colors (orange/red/green) work in both modes
    // and stay as-is. Seatbox mode keeps its orange warning background as-is.
    readonly property bool isDark: ThemeStore.isDark
    readonly property color scrimColor:    isDark ? "#000000" : "#FFFFFF"
    readonly property color cardColor:     isDark ? "#CC000000" : "#CCFFFFFF"
    readonly property color cardBorder:    isDark ? "#4DFFFFFF" : "#4D000000"
    readonly property color textPrimary:   isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#B3FFFFFF" : "#B3000000"

    // Countdown logic
    property int countdown: 15
    property bool countdownActive: false

    visible: isHibernating

    onBothBrakesHeldChanged: {
        if (hibernationOverlay.bothBrakesHeld && hibernationOverlay.isPromptMode) {
            hibernationOverlay.countdown = 15
            hibernationOverlay.countdownActive = true
            countdownTimer.start()
        } else {
            hibernationOverlay.countdownActive = false
            countdownTimer.stop()
            hibernationOverlay.countdown = 15
        }
    }

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (hibernationOverlay.countdown > 0) {
                hibernationOverlay.countdown--
            } else {
                countdownTimer.stop()
                hibernationOverlay.countdownActive = false
            }
        }
    }

    // Mode 1: Hibernation prompt (states 13, 14)
    Rectangle {
        anchors.fill: parent
        color: hibernationOverlay.scrimColor
        opacity: 0.9
        visible: hibernationOverlay.isPromptMode
    }

    Column {
        anchors.centerIn: parent
        visible: hibernationOverlay.isPromptMode
        spacing: 0

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.parent.width - 48, 420)
            height: promptContent.height + 48
            color: hibernationOverlay.cardColor
            border.width: 1
            border.color: hibernationOverlay.cardBorder
            radius: ThemeStore.radiusModal

            Column {
                id: promptContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 16

                // Power icon
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: MaterialIcon.iconPowerSettingsNew
                    font.family: "Material Icons"
                    font.pixelSize: ThemeStore.fontHero
                    color: hibernationOverlay.textPrimary
                }

                // Title
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.hibernatePrompt
                    font.pixelSize: ThemeStore.fontHeading
                    font.weight: Font.Bold
                    color: hibernationOverlay.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                // Subtitle
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.hibernateTapKeycard
                    font.pixelSize: ThemeStore.fontBody
                    color: hibernationOverlay.textPrimary
                }

                // Countdown status
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: hibernationOverlay.countdownActive && hibernationOverlay.countdown > 0
                    text: hibernationOverlay.countdown + "s"
                    font.pixelSize: ThemeStore.fontBody
                    font.weight: Font.Bold
                    color: "#FF9800"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !hibernationOverlay.countdownActive && !hibernationOverlay.bothBrakesHeld
                    text: Translations.hibernationOrHoldBrakes
                    font.pixelSize: ThemeStore.fontBody
                    color: hibernationOverlay.textSecondary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: hibernationOverlay.countdown === 0 && !hibernationOverlay.countdownActive
                    text: Translations.hibernationKeepHoldingBrakes
                    font.pixelSize: ThemeStore.fontBody
                    color: hibernationOverlay.textSecondary
                }

                // Action boxes
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 16

                    // Cancel / kickstand box
                    Rectangle {
                        width: 160
                        height: kickstandCol.height + 32
                        radius: ThemeStore.radiusModal
                        color: "#33F44336"
                        border.width: 1
                        border.color: "#80F44336"

                        Column {
                            id: kickstandCol
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: MaterialIcon.iconClose
                                font.family: "Material Icons"
                                font.pixelSize: ThemeStore.fontHeading
                                color: "#F44336"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: Translations.hibernationCancel
                                font.pixelSize: ThemeStore.fontBody
                                font.weight: Font.Bold
                                color: hibernationOverlay.textPrimary
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: Translations.hibernationKickstand
                                font.pixelSize: ThemeStore.fontBody
                                color: hibernationOverlay.textSecondary
                            }
                        }
                    }

                    // Confirm / keycard box
                    Rectangle {
                        width: 160
                        height: keycardCol.height + 32
                        radius: ThemeStore.radiusModal
                        color: "#334CAF50"
                        border.width: 1
                        border.color: "#804CAF50"

                        Column {
                            id: keycardCol
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: MaterialIcon.iconCheck
                                font.family: "Material Icons"
                                font.pixelSize: ThemeStore.fontHeading
                                color: "#4CAF50"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: Translations.hibernationConfirm
                                font.pixelSize: ThemeStore.fontBody
                                font.weight: Font.Bold
                                color: hibernationOverlay.textPrimary
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: Translations.hibernationTapKeycardToConfirm
                                font.pixelSize: ThemeStore.fontBody
                                color: hibernationOverlay.textSecondary
                            }
                        }
                    }
                }
            }
        }
    }

    // Mode 2: Seatbox warning (state 15)
    Rectangle {
        anchors.fill: parent
        color: "#FF9800"
        opacity: 0.9
        visible: hibernationOverlay.isSeatboxMode
    }

    Column {
        anchors.centerIn: parent
        visible: hibernationOverlay.isSeatboxMode
        spacing: 16

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: String.fromCodePoint(0xf02a0) // warning_amber_rounded
            font.family: "Material Icons"
            font.pixelSize: ThemeStore.fontHero
            color: "#FFFFFF"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Translations.hibernateSeatboxOpen
            font.pixelSize: ThemeStore.fontHeading
            font.weight: Font.Bold
            color: "#000000"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Translations.hibernateCloseSeatbox
            font.pixelSize: ThemeStore.fontBody
            color: "#000000"
        }
    }

    // Mode 3: Confirming (states 7, 8, 16)
    Rectangle {
        anchors.fill: parent
        color: hibernationOverlay.scrimColor
        opacity: 0.8
        visible: hibernationOverlay.isConfirmMode
    }

    Column {
        anchors.centerIn: parent
        visible: hibernationOverlay.isConfirmMode
        spacing: 16

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: MaterialIcon.iconPowerSettingsNew
            font.family: "Material Icons"
            font.pixelSize: ThemeStore.fontHero
            color: "#F44336"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Translations.hibernating
            font.pixelSize: ThemeStore.fontHeading
            font.weight: Font.Bold
            color: hibernationOverlay.textPrimary
        }

        Rectangle {
            id: confirmSpinner
            anchors.horizontalCenter: parent.horizontalCenter
            width: 32
            height: 32
            radius: ThemeStore.radiusModal
            color: "transparent"
            border.color: hibernationOverlay.textPrimary
            border.width: 3

            // Mask half the ring to fake a spinner — must match the scrim.
            Rectangle {
                width: 18
                height: 18
                color: hibernationOverlay.scrimColor
                anchors.right: parent.right
                anchors.top: parent.top
            }

            RotationAnimation on rotation {
                from: 0; to: 360
                duration: 1000
                loops: Animation.Infinite
                running: hibernationOverlay.isConfirmMode
            }
        }
    }
}

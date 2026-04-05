import QtQuick

Item {
    id: hibernationOverlay
    anchors.fill: parent

    // ScooterState enum values from Enums.h
    readonly property int stateHibernating: 7
    readonly property int stateHibernatingImminent: 8
    readonly property int stateWaitingHibernation: 13
    readonly property int stateWaitingHibernationAdvanced: 14
    readonly property int stateWaitingHibernationSeatbox: 15
    readonly property int stateWaitingHibernationConfirm: 16

    property int vehicleState: typeof vehicleStore !== "undefined" ? vehicleStore.state : 0
    property bool bothBrakesHeld: typeof vehicleStore !== "undefined"
                                  ? (vehicleStore.brakeLeft === 1 && vehicleStore.brakeRight === 1)
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

    // Countdown logic
    property int countdown: 15
    property bool countdownActive: false

    visible: isHibernating

    onBothBrakesHeldChanged: {
        if (bothBrakesHeld && isPromptMode) {
            countdown = 15
            countdownActive = true
            countdownTimer.start()
        } else {
            countdownActive = false
            countdownTimer.stop()
            countdown = 15
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
        color: "#000000"
        opacity: 0.9
        visible: isPromptMode
    }

    Column {
        anchors.centerIn: parent
        visible: isPromptMode
        spacing: 0

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.parent.width - 48, 420)
            height: promptContent.height + 48
            color: "#CC000000"
            border.width: 1
            border.color: "#4DFFFFFF"
            radius: themeStore.radiusModal

            Column {
                id: promptContent
                anchors.centerIn: parent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 24
                spacing: 16

                // Power icon
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\ue4e3" // power_settings_new
                    font.family: "Material Icons"
                    font.pixelSize: themeStore.fontHero
                    color: "#FFFFFF"
                }

                // Title
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.hibernatePrompt : ""
                    font.pixelSize: themeStore.fontHeading
                    font.bold: true
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                // Subtitle
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.hibernateTapKeycard : ""
                    font.pixelSize: themeStore.fontBody
                    color: "#FFFFFF"
                }

                // Countdown status
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: countdownActive && countdown > 0
                    text: countdown + "s"
                    font.pixelSize: themeStore.fontBody
                    font.bold: true
                    color: "#FF9800"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !countdownActive && !bothBrakesHeld
                    text: typeof translations !== "undefined" ? translations.hibernationOrHoldBrakes : ""
                    font.pixelSize: themeStore.fontBody
                    color: "#B3FFFFFF"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: countdown === 0 && !countdownActive
                    text: typeof translations !== "undefined" ? translations.hibernationKeepHoldingBrakes : ""
                    font.pixelSize: themeStore.fontBody
                    color: "#B3FFFFFF"
                }

                // Action boxes
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 16

                    // Cancel / kickstand box
                    Rectangle {
                        width: 160
                        height: kickstandCol.height + 32
                        radius: themeStore.radiusModal
                        color: "#33F44336"
                        border.width: 1
                        border.color: "#80F44336"

                        Column {
                            id: kickstandCol
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "\ue16a" // close
                                font.family: "Material Icons"
                                font.pixelSize: themeStore.fontHeading
                                color: "#F44336"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: typeof translations !== "undefined" ? translations.hibernationCancel : ""
                                font.pixelSize: themeStore.fontBody
                                font.bold: true
                                color: "#FFFFFF"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: typeof translations !== "undefined" ? translations.hibernationKickstand : ""
                                font.pixelSize: themeStore.fontBody
                                color: "#B3FFFFFF"
                            }
                        }
                    }

                    // Confirm / keycard box
                    Rectangle {
                        width: 160
                        height: keycardCol.height + 32
                        radius: themeStore.radiusModal
                        color: "#334CAF50"
                        border.width: 1
                        border.color: "#804CAF50"

                        Column {
                            id: keycardCol
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "\ue156" // check
                                font.family: "Material Icons"
                                font.pixelSize: themeStore.fontHeading
                                color: "#4CAF50"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: typeof translations !== "undefined" ? translations.hibernationConfirm : ""
                                font.pixelSize: themeStore.fontBody
                                font.bold: true
                                color: "#FFFFFF"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: typeof translations !== "undefined" ? translations.hibernationTapKeycardToConfirm : ""
                                font.pixelSize: themeStore.fontBody
                                color: "#B3FFFFFF"
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
        visible: isSeatboxMode
    }

    Column {
        anchors.centerIn: parent
        visible: isSeatboxMode
        spacing: 16

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: String.fromCodePoint(0xf02a0) // warning_amber_rounded
            font.family: "Material Icons"
            font.pixelSize: themeStore.fontHero
            color: "#FFFFFF"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: typeof translations !== "undefined" ? translations.hibernateSeatboxOpen : ""
            font.pixelSize: themeStore.fontHeading
            font.bold: true
            color: "#000000"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: typeof translations !== "undefined" ? translations.hibernateCloseSeatbox : ""
            font.pixelSize: themeStore.fontBody
            color: "#000000"
        }
    }

    // Mode 3: Confirming (states 7, 8, 16)
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.8
        visible: isConfirmMode
    }

    Column {
        anchors.centerIn: parent
        visible: isConfirmMode
        spacing: 16

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "\ue4e3" // power_settings_new
            font.family: "Material Icons"
            font.pixelSize: themeStore.fontHero
            color: "#F44336"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: typeof translations !== "undefined" ? translations.hibernating : ""
            font.pixelSize: themeStore.fontHeading
            font.bold: true
            color: "#FFFFFF"
        }

        Rectangle {
            id: confirmSpinner
            anchors.horizontalCenter: parent.horizontalCenter
            width: 32
            height: 32
            radius: themeStore.radiusModal
            color: "transparent"
            border.color: "#FFFFFF"
            border.width: 3

            Rectangle {
                width: 18
                height: 18
                color: "#000000"
                anchors.right: parent.right
                anchors.top: parent.top
            }

            RotationAnimation on rotation {
                from: 0; to: 360
                duration: 1000
                loops: Animation.Infinite
                running: isConfirmMode
            }
        }
    }
}

import QtQuick

Item {
    id: hibernationOverlay
    anchors.fill: parent

    // ScooterState enum values from Enums.h
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

    property bool isPromptMode: vehicleState === stateWaitingHibernation
                                || vehicleState === stateWaitingHibernationAdvanced
    property bool isSeatboxMode: vehicleState === stateWaitingHibernationSeatbox
    property bool isConfirmMode: vehicleState === stateWaitingHibernationConfirm

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
            color: "#CC000000" // black 0.8 opacity
            border.width: 1
            border.color: "#4DFFFFFF" // white 0.3 opacity
            radius: 16

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
                    font.pixelSize: 64
                    color: "#FFFFFF"
                }

                // Title
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Hold Both Brakes to Hibernate"
                    font.pixelSize: 28
                    font.bold: true
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                // Subtitle
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Or Tap Keycard"
                    font.pixelSize: 18
                    color: "#FFFFFF"
                }

                // Countdown status
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: countdownActive && countdown > 0
                    text: "Hold Brakes for " + countdown + " seconds"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#FF9800" // orange
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !countdownActive && !bothBrakesHeld
                    text: "Or Hold Both Brakes"
                    font.pixelSize: 18
                    color: "#B3FFFFFF" // white 70% opacity
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: countdown === 0 && !countdownActive
                    text: "Keep Holding Brakes"
                    font.pixelSize: 18
                    color: "#B3FFFFFF" // white 70% opacity
                }

                // Action boxes
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 16

                    // Kickstand box (red themed, Flutter: Icons.close + cancel/kickstand)
                    Rectangle {
                        width: 160
                        height: kickstandCol.height + 32
                        radius: 12
                        color: "#33F44336" // red 20% opacity
                        border.width: 1
                        border.color: "#80F44336" // red 50% opacity

                        Column {
                            id: kickstandCol
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "\ue16a" // close
                                font.family: "Material Icons"
                                font.pixelSize: 32
                                color: "#F44336"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Cancel"
                                font.pixelSize: 16
                                font.bold: true
                                color: "#FFFFFF"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Flip Kickstand"
                                font.pixelSize: 12
                                color: "#B3FFFFFF" // white 70%
                            }
                        }
                    }

                    // Keycard box (green themed, Flutter: Icons.check + confirm/tap keycard)
                    Rectangle {
                        width: 160
                        height: keycardCol.height + 32
                        radius: 12
                        color: "#334CAF50" // green 20% opacity
                        border.width: 1
                        border.color: "#804CAF50" // green 50% opacity

                        Column {
                            id: keycardCol
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "\ue156" // check
                                font.family: "Material Icons"
                                font.pixelSize: 32
                                color: "#4CAF50"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Confirm"
                                font.pixelSize: 16
                                font.bold: true
                                color: "#FFFFFF"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Tap Keycard"
                                font.pixelSize: 12
                                color: "#B3FFFFFF" // white 70%
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
        color: "#FF9800" // orange
        opacity: 0.9
        visible: isSeatboxMode
    }

    Column {
        anchors.centerIn: parent
        visible: isSeatboxMode
        spacing: 16

        // Warning icon (Flutter: Icons.warning_amber_rounded, size: 64)
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: String.fromCodePoint(0xf02a0) // warning_amber_rounded
            font.family: "Material Icons"
            font.pixelSize: 64
            color: "#FFFFFF"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Seatbox Open"
            font.pixelSize: 28
            font.bold: true
            color: "#FFFFFF"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Close Seatbox"
            font.pixelSize: 18
            color: "#FFFFFF"
        }
    }

    // Mode 3: Confirming (state 16)
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

        // Flutter: Icons.power_settings_new, color: Colors.red, size: 64
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "\ue4e3" // power_settings_new
            font.family: "Material Icons"
            font.pixelSize: 64
            color: "#F44336"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Hibernating..."
            font.pixelSize: 28
            font.bold: true
            color: "#FFFFFF"
        }

        // Spinner (Flutter: CircularProgressIndicator)
        Rectangle {
            id: confirmSpinner
            anchors.horizontalCenter: parent.horizontalCenter
            width: 32
            height: 32
            radius: 16
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

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: addressScreen
    color: "black"

    // Address database status constants (match C++ enum)
    readonly property int statusIdle: 0
    readonly property int statusLoading: 1
    readonly property int statusBuilding: 2
    readonly property int statusReady: 3
    readonly property int statusError: 4

    readonly property int dbStatus: typeof addressDatabase !== "undefined" ? addressDatabase.status : statusError

    // Base32 character set (excluding confusables: I, L, O, U)
    readonly property string charset: "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
    property var digits: [0, 0, 0, 0]  // 4-digit code
    property int currentDigit: 0
    property int inputState: 0  // 0=input, 1=confirm, 2=submitted
    property string errorMessage: ""

    function currentCode() {
        return charset[digits[0]] + charset[digits[1]] +
               charset[digits[2]] + charset[digits[3]]
    }

    function advanceDigit() {
        digits[currentDigit] = (digits[currentDigit] + 1) % charset.length
        digitsChanged()
    }

    function selectDigit() {
        if (inputState === 0) {
            if (currentDigit < 3) {
                currentDigit++
            } else {
                inputState = 1  // confirm
            }
        } else if (inputState === 1) {
            submitCode()
        }
    }

    function submitCode() {
        var code = currentCode()
        if (typeof addressDatabase !== "undefined") {
            var result = addressDatabase.lookupCode(code)
            if (result.valid) {
                inputState = 2
                if (typeof navigationService !== "undefined") {
                    navigationService.setDestination(result.latitude, result.longitude, code)
                }
                if (typeof screenStore !== "undefined") {
                    screenStore.setScreen(1)  // Map
                }
            } else {
                errorMessage = "Invalid code"
                errorTimer.restart()
            }
        }
    }

    Timer {
        id: errorTimer
        interval: 2000
        onTriggered: errorMessage = ""
    }

    // Brake input handling (centralized via InputHandler)
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            if (dbStatus !== statusReady) return
            if (addressScreen.inputState === 0) {
                addressScreen.advanceDigit()
            } else if (addressScreen.inputState === 1) {
                // Back to editing
                addressScreen.inputState = 0
            }
        }
        function onRightTap() {
            if (dbStatus === statusBuilding) {
                if (typeof addressDatabase !== "undefined")
                    addressDatabase.cancelBuild()
                return
            }
            if (dbStatus !== statusReady) {
                // Allow closing the screen even when not ready
                if (typeof screenStore !== "undefined")
                    screenStore.setScreen(1)
                return
            }
            addressScreen.selectDigit()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: errorMessage !== "" ? "#C62828" : "#1565C0"

            Text {
                anchors.centerIn: parent
                text: {
                    if (errorMessage !== "") return errorMessage
                    if (addressScreen.inputState === 1) return "CONFIRM DESTINATION"
                    return "ENTER DESTINATION CODE"
                }
                color: "white"
                font.pixelSize: 24
                font.bold: true
            }
        }

        Item { Layout.fillHeight: true }

        // Loading / Building state
        Loader {
            Layout.alignment: Qt.AlignHCenter
            active: dbStatus === statusLoading || dbStatus === statusBuilding
            sourceComponent: ColumnLayout {
                spacing: 16

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof addressDatabase !== "undefined" ? addressDatabase.statusMessage : ""
                    color: "white"
                    font.pixelSize: 16
                }

                // Progress bar (only during building)
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    visible: dbStatus === statusBuilding
                    width: 200
                    height: 6
                    radius: 3
                    color: "#333333"

                    Rectangle {
                        width: parent.width * (typeof addressDatabase !== "undefined" ? addressDatabase.buildProgress : 0)
                        height: parent.height
                        radius: 3
                        color: "#1565C0"
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: dbStatus === statusBuilding
                    text: typeof addressDatabase !== "undefined"
                          ? Math.round(addressDatabase.buildProgress * 100) + "%"
                          : "0%"
                    color: Qt.rgba(1, 1, 1, 0.7)
                    font.pixelSize: 13
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: dbStatus === statusBuilding && typeof addressDatabase !== "undefined" && addressDatabase.addressCount > 0
                    text: typeof addressDatabase !== "undefined" ? addressDatabase.addressCount + " addresses found" : ""
                    color: Qt.rgba(1, 1, 1, 0.6)
                    font.pixelSize: 13
                }
            }
        }

        // Error state
        Loader {
            Layout.alignment: Qt.AlignHCenter
            active: dbStatus === statusError
            sourceComponent: Text {
                text: typeof addressDatabase !== "undefined" ? addressDatabase.statusMessage : "Address database unavailable"
                color: "#EF5350"
                font.pixelSize: 16
            }
        }

        // Code input (only when database is ready)
        Loader {
            Layout.alignment: Qt.AlignHCenter
            active: dbStatus === statusReady
            sourceComponent: RowLayout {
                spacing: 12

                Repeater {
                    model: 4

                    Rectangle {
                        width: 64
                        height: 80
                        radius: 8
                        color: {
                            if (addressScreen.inputState === 1) return "#2E7D32"
                            return index === addressScreen.currentDigit ? "#1565C0" : "#333333"
                        }
                        border.width: index === addressScreen.currentDigit && addressScreen.inputState === 0 ? 2 : 0
                        border.color: "white"

                        Text {
                            anchors.centerIn: parent
                            text: addressScreen.charset[addressScreen.digits[index]]
                            font.pixelSize: 24
                            font.bold: true
                            color: "white"
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 20 }

        // Instructions
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: {
                if (dbStatus === statusBuilding) return "Right brake: Cancel"
                if (dbStatus !== statusReady) return "Right brake: Close"
                if (addressScreen.inputState === 1) return "Right brake: Confirm | Left brake: Edit"
                return "Left brake: Scroll | Right brake: Next"
            }
            font.pixelSize: 13
            color: Qt.rgba(1, 1, 1, 0.6)
        }

        Item { Layout.fillHeight: true }

        // Footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#111111"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16

                Text {
                    text: dbStatus === statusReady ? "L: Scroll" : ""
                    color: Qt.rgba(1, 1, 1, 0.6)
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: {
                        if (dbStatus === statusBuilding) return "R: Cancel"
                        if (dbStatus !== statusReady) return "R: Close"
                        return "R: " + (addressScreen.inputState === 1 ? "Submit" : "Next")
                    }
                    color: Qt.rgba(1, 1, 1, 0.6)
                    font.pixelSize: 11
                }
            }
        }
    }
}

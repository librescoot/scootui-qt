import QtQuick
import QtQuick.Layouts

Rectangle {
    id: addressScreen
    color: "black"

    // Base32 character set (excluding confusables: I, L, O, U)
    readonly property string charset: "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
    property var digits: [0, 0, 0, 0]  // 4-digit code
    property int currentDigit: 0
    property int state: 0  // 0=input, 1=confirm, 2=submitted

    function currentCode() {
        return charset[digits[0]] + charset[digits[1]] +
               charset[digits[2]] + charset[digits[3]]
    }

    function advanceDigit() {
        digits[currentDigit] = (digits[currentDigit] + 1) % charset.length
        digitsChanged()
    }

    function selectDigit() {
        if (state === 0) {
            if (currentDigit < 3) {
                currentDigit++
            } else {
                state = 1  // confirm
            }
        } else if (state === 1) {
            submitCode()
        }
    }

    function submitCode() {
        state = 2
        var code = currentCode()
        // Decode base32 to lat/lng (simplified - actual decoding depends on encoding scheme)
        if (typeof navigationService !== "undefined") {
            navigationService.setDestination(0, 0, code)
        }
        // Return to map screen
        if (typeof screenStore !== "undefined") {
            screenStore.setScreen(1)  // Map
        }
    }

    // Brake input handling (centralized via InputHandler)
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            if (addressScreen.state === 0) {
                addressScreen.advanceDigit()
            } else if (addressScreen.state === 1) {
                // Back to editing
                addressScreen.state = 0
            }
        }
        function onRightTap() {
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
            color: "#1565C0"

            Text {
                anchors.centerIn: parent
                text: addressScreen.state === 1 ? "CONFIRM DESTINATION" : "ENTER DESTINATION CODE"
                color: "white"
                font.pixelSize: 14
                font.bold: true
            }
        }

        Item { Layout.fillHeight: true }

        // Code display
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Repeater {
                model: 4

                Rectangle {
                    width: 64
                    height: 80
                    radius: 8
                    color: {
                        if (addressScreen.state === 1) return "#2E7D32"
                        return index === addressScreen.currentDigit ? "#1565C0" : "#333333"
                    }
                    border.width: index === addressScreen.currentDigit && addressScreen.state === 0 ? 2 : 0
                    border.color: "white"

                    Text {
                        anchors.centerIn: parent
                        text: addressScreen.charset[addressScreen.digits[index]]
                        font.pixelSize: 40
                        font.bold: true
                        color: "white"
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 20 }

        // Instructions
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: {
                if (addressScreen.state === 1) return "Right brake: Confirm | Left brake: Edit"
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
                    text: "L: Scroll"
                    color: Qt.rgba(1, 1, 1, 0.4)
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "R: " + (addressScreen.state === 1 ? "Submit" : "Next")
                    color: Qt.rgba(1, 1, 1, 0.4)
                    font.pixelSize: 11
                }
            }
        }
    }
}

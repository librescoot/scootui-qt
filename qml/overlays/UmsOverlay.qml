import QtQuick

Item {
    id: umsOverlay
    anchors.fill: parent

    property string usbStatus: typeof usbStore !== "undefined" ? usbStore.status : "idle"

    visible: opacity > 0
    opacity: (usbStatus !== "idle" && usbStatus !== "") ? 1.0 : 0.0

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    Column {
        anchors.centerIn: parent
        spacing: 24

        // Preparing state
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "preparing"
            text: "Preparing Storage"
            font.pixelSize: 20
            font.bold: true
            color: "#FFFFFF"
        }

        // Active state
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "active"
            spacing: 24

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "USB"
                font.pixelSize: 64
                font.bold: true
                color: "#FFFFFF"
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "USB Mass Storage"
                    font.pixelSize: 24
                    font.bold: true
                    color: "#FFFFFF"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Connect to Computer"
                    font.pixelSize: 16
                    color: "#B3FFFFFF" // white 70% opacity
                }
            }
        }

        // Processing state
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "processing"
            text: "Processing Files"
            font.pixelSize: 20
            font.bold: true
            color: "#FFFFFF"
        }

        // Default/other state
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus !== "preparing" && usbStatus !== "active"
                     && usbStatus !== "processing" && usbStatus !== "idle"
                     && usbStatus !== ""
            text: usbStatus
            font.pixelSize: 20
            font.bold: true
            color: "#FFFFFF"
        }
    }
}

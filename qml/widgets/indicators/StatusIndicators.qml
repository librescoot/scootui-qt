import QtQuick

Row {
    id: statusIndicators
    spacing: 4
    layoutDirection: Qt.RightToLeft

    readonly property int gpsState: typeof gpsStore !== "undefined" ? gpsStore.gpsState : 0
    readonly property int btStatus: typeof bluetoothStore !== "undefined" ? bluetoothStore.status : 1
    readonly property int modemState: typeof internetStore !== "undefined" ? internetStore.modemState : 0
    readonly property int cloudStatus: typeof internetStore !== "undefined" ? internetStore.unuCloud : 1
    readonly property int signalQuality: typeof internetStore !== "undefined" ? internetStore.signalQuality : 0
    readonly property string accessTech: typeof internetStore !== "undefined" ? internetStore.accessTech : ""
    readonly property int vehicleState: typeof vehicleStore !== "undefined" ? vehicleStore.state : 0
    readonly property bool otaActive: typeof otaStore !== "undefined" ? otaStore.isActive : false
    readonly property string otaDbcStatus: typeof otaStore !== "undefined" ? otaStore.dbcStatus : "idle"
    readonly property int otaDbcDownloadProgress: typeof otaStore !== "undefined" ? otaStore.dbcDownloadProgress : 0
    readonly property int otaDbcInstallProgress: typeof otaStore !== "undefined" ? otaStore.dbcInstallProgress : 0

    function accessTechLabel(tech) {
        switch (tech) {
            case "5G": return "5G"
            case "LTE":
            case "4G": return "4G"
            case "3G":
            case "UMTS":
            case "HSPA":
            case "HSDPA":
            case "HSUPA": return "3G"
            case "2G":
            case "EDGE":
            case "GPRS": return "2G"
            case "GSM": return "G"
            default: return ""
        }
    }

    // OTA status indicator (appears to the right of cloud icon in RTL layout)
    Item {
        width: 20; height: 20
        visible: otaActive && (vehicleState === 2 || vehicleState === 4)

        Image {
            id: otaIcon
            anchors.fill: parent
            sourceSize: Qt.size(20, 20)
            source: {
                switch (otaDbcStatus) {
                    case "downloading":
                        return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
                    case "installing":
                        return "qrc:/ScootUI/assets/icons/librescoot-ota-status-installing.svg"
                    case "rebooting":
                    case "reboot-failed":
                        return "qrc:/ScootUI/assets/icons/librescoot-ota-status-waiting-for-reboot.svg"
                    case "error":
                    case "error-failed":
                        return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
                    default:
                        return ""
                }
            }
        }

        // Error overlay
        Image {
            anchors.fill: parent
            sourceSize: Qt.size(20, 20)
            source: "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
            visible: otaDbcStatus === "error" || otaDbcStatus === "error-failed"
        }

        // Progress text
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            font.pixelSize: 7
            font.bold: true
            color: "white"
            visible: otaDbcStatus === "downloading" || otaDbcStatus === "installing"
            text: {
                if (otaDbcStatus === "downloading")
                    return otaDbcDownloadProgress + "%"
                if (otaDbcStatus === "installing")
                    return otaDbcInstallProgress + "%"
                return ""
            }
        }
    }

    // Cloud status icon
    Image {
        width: 20; height: 20
        sourceSize: Qt.size(20, 20)
        source: cloudStatus === 0
            ? "qrc:/ScootUI/assets/icons/librescoot-internet-cloud-connected.svg"
            : "qrc:/ScootUI/assets/icons/librescoot-internet-cloud-disconnected.svg"
        visible: true
    }

    // Internet/modem icon with access tech overlay
    Item {
        width: 20; height: 20

        Image {
            anchors.fill: parent
            sourceSize: Qt.size(20, 20)
            source: {
                if (modemState === 0) return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-off.svg"
                if (modemState === 1) return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-disconnected.svg"
                // Connected - show signal bars
                var bars = Math.min(Math.floor(signalQuality / 20), 4)
                return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-connected-" + bars + ".svg"
            }
        }

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            font.pixelSize: 7
            font.bold: true
            color: "white"
            visible: modemState >= 2 && accessTech !== ""
            text: accessTechLabel(accessTech)
        }
    }

    // Bluetooth icon
    Image {
        width: 20; height: 20
        sourceSize: Qt.size(20, 20)
        source: btStatus === 0
            ? "qrc:/ScootUI/assets/icons/librescoot-bluetooth-connected.svg"
            : "qrc:/ScootUI/assets/icons/librescoot-bluetooth-disconnected.svg"
    }

    // GPS icon
    Image {
        width: 20; height: 20
        sourceSize: Qt.size(20, 20)
        source: {
            // GpsState: 0=Off, 1=Searching, 2=FixEstablished, 3=Error
            switch (gpsState) {
                case 0: return "qrc:/ScootUI/assets/icons/librescoot-gps-off.svg"
                case 1: return "qrc:/ScootUI/assets/icons/librescoot-gps-searching.svg"
                case 2: return "qrc:/ScootUI/assets/icons/librescoot-gps-fix-established.svg"
                case 3: return "qrc:/ScootUI/assets/icons/librescoot-gps-error.svg"
                default: return "qrc:/ScootUI/assets/icons/librescoot-gps-off.svg"
            }
        }
    }
}

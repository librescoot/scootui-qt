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

    // Cloud status icon
    Image {
        width: 20; height: 20
        sourceSize: Qt.size(20, 20)
        source: cloudStatus === 0
            ? "qrc:/ScootUI/assets/icons/librescoot-internet-cloud-connected.svg"
            : "qrc:/ScootUI/assets/icons/librescoot-internet-cloud-disconnected.svg"
        visible: true
    }

    // Internet/modem icon
    Image {
        width: 20; height: 20
        sourceSize: Qt.size(20, 20)
        source: {
            if (modemState === 0) return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-off.svg"
            if (modemState === 1) return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-disconnected.svg"
            // Connected - show signal bars
            var bars = Math.min(Math.floor(signalQuality / 20), 4)
            return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-connected-" + bars + ".svg"
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

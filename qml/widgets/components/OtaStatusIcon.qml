import QtQuick

Item {
    id: root

    property string status: "idle"
    property int size: 64
    // The ota-status SVGs are baked fill="white", so they only read correctly on
    // a dark surface. Tint them like every other neutral icon rather than
    // relying on the artwork's own colour. Default stays white so the existing
    // black-background callers (ShutdownOverlay) are
    // unchanged; a theme-aware caller passes its own foreground.
    property color tintColor: "#FFFFFF"

    width: size
    height: size

    TintedImage {
        anchors.fill: parent
        sourceSize: Qt.size(root.size, root.size)
        tintColor: root.tintColor
        source: {
            switch (root.status) {
                case "downloading":
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
                case "preparing":
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-preparing.svg"
                case "installing":
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-installing.svg"
                case "pending-reboot":
                case "rebooting":
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-waiting-for-reboot.svg"
                case "error":
                case "error-failed":
                case "reboot-failed":
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-error.svg"
                default:
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
            }
        }
    }
}

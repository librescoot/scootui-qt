import QtQuick

Item {
    id: root

    property string status: "idle"
    property int size: 64

    width: size
    height: size

    Image {
        anchors.fill: parent
        sourceSize: Qt.size(root.size, root.size)
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

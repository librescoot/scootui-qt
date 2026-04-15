import QtQuick

Item {
    id: root

    property string status: "idle"
    property int size: 64

    width: size
    height: size

    readonly property bool isError: status === "error" || status === "error-failed"

    Image {
        anchors.fill: parent
        sourceSize: Qt.size(root.size, root.size)
        source: {
            switch (root.status) {
                case "downloading":
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
                case "preparing":
                case "installing":
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-installing.svg"
                case "pending-reboot":
                case "rebooting":
                case "reboot-failed":
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-waiting-for-reboot.svg"
                default:
                    return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
            }
        }
    }

    Image {
        anchors.fill: parent
        sourceSize: Qt.size(root.size, root.size)
        source: "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
        visible: root.isError
    }
}

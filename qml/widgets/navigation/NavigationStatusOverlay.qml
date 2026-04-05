import QtQuick
import QtQuick.Layouts
import "../../theme"

Item {
    id: navStatusOverlay
    anchors.fill: parent

    // Navigation status enum values (must match NavigationStatus in C++)
    readonly property int statusIdle: 0
    readonly property int statusCalculating: 1
    readonly property int statusNavigating: 2
    readonly property int statusRerouting: 3
    readonly property int statusArrived: 4
    readonly property int statusError: 5

    property int navStatus: typeof navigationService !== "undefined"
                            ? navigationService.status : 0

    visible: navStatus === statusCalculating ||
             navStatus === statusRerouting ||
             navStatus === statusArrived ||
             navStatus === statusError

    // Floating status pill at top-center
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 50
        width: statusRow.width + 24
        height: statusRow.height + 16
        radius: height / 2
        color: {
            switch (navStatusOverlay.navStatus) {
                case statusCalculating:
                case statusRerouting: return "#E65100"  // orange
                case statusArrived: return "#2E7D32"    // green
                case statusError: return "#C62828"      // red
                default: return "transparent"
            }
        }
        opacity: 0.9

        RowLayout {
            id: statusRow
            anchors.centerIn: parent
            spacing: 8

            // Spinner for calculating/rerouting
            Rectangle {
                id: spinner
                width: 16
                height: 16
                radius: Theme.radiusCard
                color: "transparent"
                border.color: "white"
                border.width: 2
                visible: navStatusOverlay.navStatus === statusCalculating ||
                         navStatusOverlay.navStatus === statusRerouting

                Rectangle {
                    width: 10
                    height: 10
                    color: {
                        switch (navStatusOverlay.navStatus) {
                            case statusCalculating:
                            case statusRerouting: return "#E65100"
                            default: return "transparent"
                        }
                    }
                    anchors.right: parent.right
                    anchors.top: parent.top
                }

                RotationAnimation on rotation {
                    from: 0; to: 360
                    duration: 1000
                    loops: Animation.Infinite
                    running: spinner.visible
                }
            }

            // Arrived icon (Flutter: Icons.place, green, size 24)
            Text {
                visible: navStatusOverlay.navStatus === statusArrived
                text: "\ue4c9" // place
                font.family: "Material Icons"
                font.pixelSize: Theme.fontTitle
                color: "#FFFFFF"
            }

            // Error icon (Flutter: warning_amber)
            Text {
                visible: navStatusOverlay.navStatus === statusError
                text: "\ue6cc" // warning_amber
                font.family: "Material Icons"
                font.pixelSize: Theme.fontBody
                color: "white"
            }

            Text {
                text: {
                    switch (navStatusOverlay.navStatus) {
                        case statusCalculating: return "Calculating route..."
                        case statusRerouting: return "Recalculating..."
                        case statusArrived: return "You have arrived"
                        case statusError:
                            return typeof navigationService !== "undefined"
                                   ? navigationService.errorMessage : "Route error"
                        default: return ""
                    }
                }
                font.pixelSize: Theme.fontBody
                font.bold: true
                color: "white"
            }
        }
    }

    // Off-route indicator (when navigating but off route)
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 50
        width: offRouteRow.width + 20
        height: offRouteRow.height + 12
        radius: height / 2
        color: "#E65100"
        opacity: 0.85
        visible: navStatusOverlay.navStatus === statusNavigating &&
                 typeof navigationService !== "undefined" && navigationService.isOffRoute

        RowLayout {
            id: offRouteRow
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: "\ue6cc" // warning_amber
                font.family: "Material Icons"
                font.pixelSize: Theme.fontBody
                color: "white"
            }

            Text {
                text: "Off route"
                font.pixelSize: Theme.fontBody
                font.bold: true
                color: "white"
            }
        }
    }
}

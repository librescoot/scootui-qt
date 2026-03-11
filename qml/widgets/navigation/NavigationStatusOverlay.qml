import QtQuick
import QtQuick.Layouts

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
                radius: 8
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

            // Arrived icon (checkmark)
            Canvas {
                width: 16; height: 16
                visible: navStatusOverlay.navStatus === statusArrived
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "white"
                    ctx.lineWidth = 2.5
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(2, 9)
                    ctx.lineTo(6, 13)
                    ctx.lineTo(14, 3)
                    ctx.stroke()
                }
            }

            // Error icon (warning triangle)
            Canvas {
                width: 16; height: 16
                visible: navStatusOverlay.navStatus === statusError
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    // Triangle
                    ctx.fillStyle = "white"
                    ctx.beginPath()
                    ctx.moveTo(8, 1)
                    ctx.lineTo(15, 15)
                    ctx.lineTo(1, 15)
                    ctx.closePath()
                    ctx.fill()
                    // Exclamation mark
                    ctx.fillStyle = "#C62828"
                    ctx.fillRect(7, 5, 2, 5)
                    ctx.fillRect(7, 12, 2, 2)
                }
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
                font.pixelSize: 14
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

            Canvas {
                width: 14; height: 14
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = "white"
                    ctx.beginPath()
                    ctx.moveTo(7, 1)
                    ctx.lineTo(13, 13)
                    ctx.lineTo(1, 13)
                    ctx.closePath()
                    ctx.fill()
                    ctx.fillStyle = "#E65100"
                    ctx.fillRect(6, 4, 2, 4.5)
                    ctx.fillRect(6, 10, 2, 2)
                }
            }

            Text {
                text: "Off route"
                font.pixelSize: 13
                font.bold: true
                color: "white"
            }
        }
    }
}

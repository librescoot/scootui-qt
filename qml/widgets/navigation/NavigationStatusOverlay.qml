import QtQuick
import QtQuick.Layouts
import "../components"

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
                case statusRerouting: return themeStore.statusNeutral
                case statusArrived: return themeStore.statusSuccess
                case statusError: return themeStore.statusError
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
                radius: themeStore.radiusCard
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
                            case statusRerouting: return themeStore.statusNeutral
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
                text: MaterialIcon.iconPlace
                font.family: "Material Icons"
                font.pixelSize: themeStore.fontTitle
                color: "#FFFFFF"
            }

            // Error icon (Flutter: warning_amber)
            Text {
                visible: navStatusOverlay.navStatus === statusError
                text: MaterialIcon.iconWarningAmber
                font.family: "Material Icons"
                font.pixelSize: themeStore.fontBody
                color: "white"
            }

            Text {
                text: {
                    switch (navStatusOverlay.navStatus) {
                        case statusCalculating: return translations.navCalculating
                        case statusRerouting: return translations.navRecalculating
                        case statusArrived: return translations.navArrived
                        case statusError:
                            return typeof navigationService !== "undefined"
                                   ? navigationService.errorMessage : translations.navRouteError
                        default: return ""
                    }
                }
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
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
        color: themeStore.statusWarning
        opacity: 0.85
        visible: navStatusOverlay.navStatus === statusNavigating &&
                 typeof navigationService !== "undefined" && navigationService.isOffRoute

        RowLayout {
            id: offRouteRow
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: MaterialIcon.iconWarningAmber
                font.family: "Material Icons"
                font.pixelSize: themeStore.fontBody
                color: "white"
            }

            Text {
                text: translations.navOffRoute
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
                color: "white"
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts
import "../widgets/navigation"
import "../widgets/components"

Item {
    id: dock
    property var service: typeof notificationService !== "undefined" ? notificationService : null
    property bool isDark: typeof themeStore !== "undefined" && themeStore ? themeStore.isDark : true
    property var presentation: service ? service.presentation : ({})
    property var main: presentation.main || ({})
    property var companion: presentation.companion || ({})
    readonly property bool hasMain: main && Object.keys(main).length > 0
    readonly property bool hasCompanion: companion && Object.keys(companion).length > 0

    visible: hasMain || hasCompanion
    height: service ? service.occupiedHeight : 0
    width: parent ? parent.width : 480
    clip: true

    readonly property color criticalColor: isDark ? "#b71c1c" : "#c62828"
    readonly property color warningColor: isDark ? "#604610" : "#fff1d1"
    readonly property color foreground: "#ffffff"
    readonly property color quietForeground: isDark ? "#c9d0d6" : "#4b5560"

    Rectangle {
        anchors.fill: parent
        color: isDark ? "#202a31" : "#f5f7f8"
        border.color: isDark ? "#40505b" : "#d7dce1"
        border.width: 1
    }

    Loader {
        id: maneuver
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 96
        active: dock.hasMain && dock.main.kind === "nav" && dock.main.status === 2
                && dock.main.priority === 1
        sourceComponent: TurnByTurnWidget {}
    }

    Rectangle {
        id: mainCard
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: dock.hasCompanion ? (dock.main.priority === 0 ? 94 : 58) : dock.height
        visible: dock.hasMain && !(dock.main.kind === "nav" && dock.main.status === 2
                                    && dock.main.priority === 1)
        color: dock.main.priority === 0 ? dock.criticalColor
             : dock.main.priority === 2 ? dock.warningColor
             : (isDark ? "#29333c" : "#ffffff")

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Text {
                Layout.preferredWidth: 32
                visible: dock.main.kind === "nav"
                text: {
                    if (dock.main.status === 4)
                        return MaterialIcon.iconPlace
                    switch (dock.main.maneuverType) {
                    case 4: return MaterialIcon.iconTurnLeft
                    case 5: return MaterialIcon.iconTurnRight
                    case 8: return MaterialIcon.iconTurnSharpLeft
                    case 9: return MaterialIcon.iconTurnSharpRight
                    case 10: case 11: return MaterialIcon.iconUTurnLeft
                    case 20: case 21: case 22: return MaterialIcon.iconFlag
                    default: return MaterialIcon.iconStraight
                    }
                }
                font.family: "Material Icons"
                font.pixelSize: 28
                color: dock.main.priority === 0 ? foreground : (isDark ? "#73c7ff" : "#1976b8")
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: {
                        if (dock.main.kind === "nav") {
                            if (dock.main.status === 2 && dock.main.priority === 3)
                                return dock.main.instruction || "Navigation"
                            if (dock.main.status === 1)
                                return typeof translations !== "undefined" ? translations.navCalculating : "Calculating route"
                            if (dock.main.status === 3)
                                return typeof translations !== "undefined" ? translations.navRecalculating : "Recalculating route"
                            if (dock.main.status === 4)
                                return typeof translations !== "undefined" ? translations.navArrived : "Arrived"
                            if (dock.main.offRoute)
                                return typeof translations !== "undefined" ? translations.navOffRoute : "Off route"
                            return dock.main.instruction || "Navigation"
                        }
                        if (dock.main.id === "map-coverage")
                            return typeof translations !== "undefined" ? translations.mapOutOfCoverage : "No map for current location"
                        return dock.main.title || "Notification"
                    }
                    color: dock.main.priority === 0 ? foreground
                         : dock.main.priority === 2 ? (isDark ? "#ffe0a3" : "#6b4300")
                         : (isDark ? "#f3f6f8" : "#1e2930")
                    font.pixelSize: 17
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    visible: dock.main.kind !== "nav"
                    text: dock.main.id === "map-coverage"
                          ? "" : (dock.main.body || "")
                    color: dock.main.priority === 0 ? foreground : quietForeground
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                visible: dock.main.priority === 0 && (dock.presentation.criticalCount || 0) > 1
                text: "+" + ((dock.presentation.criticalCount || 1) - 1)
                color: foreground
                font.pixelSize: 16
                font.weight: Font.Bold
            }
        }
    }

    Rectangle {
        id: navCompanion
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 34
        visible: dock.hasCompanion
        color: isDark ? "#1d2931" : "#edf4f8"
        border.color: isDark ? "#3d4e5a" : "#c5dae7"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8
            Text {
                text: {
                    switch (dock.companion.maneuverType) {
                    case 4: return MaterialIcon.iconTurnLeft
                    case 5: return MaterialIcon.iconTurnRight
                    case 8: return MaterialIcon.iconTurnSharpLeft
                    case 9: return MaterialIcon.iconTurnSharpRight
                    case 10: case 11: return MaterialIcon.iconUTurnLeft
                    case 20: case 21: case 22: return MaterialIcon.iconFlag
                    default: return MaterialIcon.iconStraight
                    }
                }
                font.family: "Material Icons"
                font.pixelSize: 24
                color: isDark ? "#73c7ff" : "#1976b8"
            }
            Text {
                Layout.fillWidth: true
                text: (dock.companion.distance || 0).toFixed(0) + " m  ·  "
                      + (dock.companion.street || "Navigation")
                color: isDark ? "#dbe8f0" : "#274250"
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }
    }
}

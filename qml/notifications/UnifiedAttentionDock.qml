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
    readonly property bool hasBody: main.kind !== "nav" && main.id !== "map-coverage"
                                   && !!main.body

    visible: hasMain || hasCompanion
    height: service ? service.occupiedHeight : 0
    width: parent ? parent.width : 480
    clip: true

    readonly property color criticalColor: isDark ? "#481c20" : "#fee9e7"
    readonly property color warningColor: isDark ? "#382d19" : "#fff1d1"
    readonly property color foreground: isDark ? "#ffffff" : "#8f1717"
    readonly property color quietForeground: isDark ? "#c9d0d6" : "#4b5560"
    readonly property color accent: main.priority === 0 ? (isDark ? "#ff827b" : "#bc2929")
                                   : main.priority === 2 ? (isDark ? "#ffd075" : "#8a5800")
                                   : (isDark ? "#73c7ff" : "#1976b8")

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
        height: dock.height - (dock.hasCompanion ? navCompanion.height : 0)
        visible: dock.hasMain && !(dock.main.kind === "nav" && dock.main.status === 2
                                    && dock.main.priority === 1)
        color: dock.main.priority === 0 ? dock.criticalColor
             : dock.main.priority === 2 ? dock.warningColor
             : (isDark ? "#29333c" : "#ffffff")

        Rectangle {
            width: 3
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: dock.accent
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            // Two title lines plus details must leave room for the navigation companion.
            anchors.topMargin: dock.hasBody ? 6 : 10
            anchors.bottomMargin: anchors.topMargin
            spacing: 10

            Text {
                objectName: "notificationIcon"
                Layout.preferredWidth: 26
                Layout.alignment: Qt.AlignVCenter
                text: {
                    if (dock.main.kind !== "nav") {
                        if (dock.main.priority === 0 || dock.main.kind === "error")
                            return MaterialIcon.iconErrorOutline
                        if (dock.main.priority === 2)
                            return MaterialIcon.iconWarningAmber
                        if (dock.main.id === "map-update")
                            return MaterialIcon.iconUpdate
                        if (dock.main.kind === "success")
                            return MaterialIcon.iconCheckCircleOutline
                        return MaterialIcon.iconFlag
                    }
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
                font.pixelSize: 26
                color: dock.accent
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Item {
                    Layout.fillWidth: true
                    implicitHeight: titleText.height

                    BalancedText {
                        id: titleText
                        objectName: "notificationTitle"
                        maxWidth: parent.width
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
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                        maximumLineCount: dock.main.priority === 0 || !dock.hasBody ? 2 : 1
                        lineHeightMode: Text.FixedHeight
                        lineHeight: 28
                        elide: Text.ElideRight
                    }
                }
                Text {
                    objectName: "notificationBody"
                    Layout.fillWidth: true
                    visible: dock.hasBody
                    text: dock.main.id === "map-coverage"
                          ? "" : (dock.main.body || "")
                    color: dock.main.priority === 0 ? foreground : quietForeground
                    font.pixelSize: 20
                    lineHeightMode: Text.FixedHeight
                    lineHeight: 24
                    elide: Text.ElideRight
                }
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                visible: dock.main.priority === 0 && (dock.presentation.criticalCount || 0) > 1
                text: "+" + ((dock.presentation.criticalCount || 1) - 1)
                color: foreground
                font.pixelSize: 24
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
            anchors.leftMargin: 8
            anchors.rightMargin: 8
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
                objectName: "notificationCompanion"
                font.pixelSize: 20
                elide: Text.ElideRight
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts
import "../components"
import ScootUI 1.0

Item {
    id: tbtWidget
    // Grow downward to fit the instruction, capped at ~75% of the parent so it
    // never eats the whole screen. Floor at 96 keeps the layout stable when idle.
    height: visible
            ? Math.min(parent ? parent.height * 0.75 : Number.MAX_VALUE,
                       Math.max(contentCol.implicitHeight + 24, 96))
            : 0
    visible: NavigationService.isNavigating
             && NavigationService.currentManeuverDistance > 0

    property bool isDark: ThemeStore.isDark

    // Maneuver type enum values (must match ManeuverType in C++)
    readonly property int mtOther: 0
    readonly property int mtKeepStraight: 1
    readonly property int mtKeepLeft: 2
    readonly property int mtKeepRight: 3
    readonly property int mtTurnLeft: 4
    readonly property int mtTurnRight: 5
    readonly property int mtTurnSlightLeft: 6
    readonly property int mtTurnSlightRight: 7
    readonly property int mtTurnSharpLeft: 8
    readonly property int mtTurnSharpRight: 9
    readonly property int mtUTurn: 10
    readonly property int mtUTurnRight: 11
    readonly property int mtExitLeft: 12
    readonly property int mtExitRight: 13
    readonly property int mtMergeStraight: 14
    readonly property int mtMergeLeft: 15
    readonly property int mtMergeRight: 16
    readonly property int mtRoundaboutEnter: 17
    readonly property int mtRoundaboutExit: 18
    readonly property int mtFerry: 19

    function iconThreshold(maneuverType) {
        switch (maneuverType) {
            case mtUTurn: case mtUTurnRight: return 600
            case mtRoundaboutEnter: case mtRoundaboutExit:
            case mtTurnSharpLeft: case mtTurnSharpRight: return 500
            case mtTurnLeft: case mtTurnRight:
            case mtExitLeft: case mtExitRight: return 400
            case mtTurnSlightLeft: case mtTurnSlightRight:
            case mtMergeStraight: case mtMergeLeft: case mtMergeRight: return 300
            case mtKeepStraight: case mtKeepLeft: case mtKeepRight: return 150
            default: return 1000
        }
    }

    function formatDistance(meters) {
        if (meters >= 1000) return (meters / 1000).toFixed(1) + " km"
        if (meters >= 100) return (Math.round(meters / 100) * 100) + " m"
        if (meters >= 10) return (Math.round(meters / 10) * 10) + " m"
        return Math.round(meters) + " m"
    }

    function maneuverIcon(maneuverType) {
        switch (maneuverType) {
            case mtTurnLeft:                          return MaterialIcon.iconTurnLeft
            case mtTurnSharpLeft:                     return MaterialIcon.iconTurnSharpLeft
            case mtTurnRight:                         return MaterialIcon.iconTurnRight
            case mtTurnSharpRight:                    return MaterialIcon.iconTurnSharpRight
            case mtTurnSlightLeft: case mtKeepLeft:   return MaterialIcon.iconTurnSlightLeft
            case mtTurnSlightRight: case mtKeepRight: return MaterialIcon.iconTurnSlightRight
            case mtUTurn:                             return MaterialIcon.iconUTurnLeft
            case mtUTurnRight:                        return MaterialIcon.iconUTurnRight
            case mtExitLeft:                          return MaterialIcon.iconTurnSlightLeft
            case mtExitRight:                         return MaterialIcon.iconTurnSlightRight
            case mtMergeStraight: case mtMergeLeft: case mtMergeRight:
                                                      return MaterialIcon.iconMerge
            case mtKeepStraight: case mtFerry:        return MaterialIcon.iconStraight
            default:                                  return MaterialIcon.iconStraight
        }
    }

    // Main background container
    Rectangle {
        anchors.fill: parent
        color: isDark ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.8)

        // Bottom border
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.12)
        }

        RowLayout {
            id: contentRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            // Icon box (left-aligned)
            Item {
                Layout.preferredWidth: 80
                Layout.preferredHeight: 80

                property int mType: NavigationService.currentManeuverType
                property double mDist: NavigationService.currentManeuverDistance
                property bool isRoundabout: (mType === mtRoundaboutEnter || mType === mtRoundaboutExit)
                                            && mDist <= iconThreshold(mType)

                Loader {
                    anchors.centerIn: parent
                    active: parent.isRoundabout
                    sourceComponent: RoundaboutIcon {
                        exitNumber: true
                                    ? Math.max(1, NavigationService.roundaboutExitCount) : 1
                        isDark: tbtWidget.isDark
                        size: 64
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: !parent.isRoundabout
                    text: parent.mDist <= iconThreshold(parent.mType)
                          ? maneuverIcon(parent.mType) : MaterialIcon.iconStraight
                    font.family: "Material Icons"
                    font.pixelSize: ThemeStore.fontHero
                    color: isDark ? "white" : "#212121"
                }
            }

            // Text Column (center-expanded)
            ColumnLayout {
                id: contentCol
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.topMargin: 12
                Layout.bottomMargin: 8
                spacing: 4

                // Distance indicator
                Text {
                    Layout.fillWidth: true
                    text: true
                          ? formatDistance(NavigationService.currentManeuverDistance) : ""
                    font.pixelSize: ThemeStore.fontBody
                    font.weight: Font.Bold
                    color: isDark ? "white" : "#212121"
                    lineHeight: 1.0
                }

                // Main instruction text (verbal) — wraps freely; widget grows to fit
                Text {
                    Layout.fillWidth: true
                    text: NavigationService.currentVerbalInstruction
                    font.pixelSize: ThemeStore.fontBody
                    font.weight: isDark ? Font.Normal : Font.Medium
                    color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    wrapMode: Text.WordWrap
                    lineHeight: 1.2
                }

                // Next instruction preview
                RowLayout {
                    Layout.fillWidth: true
                    visible: NavigationService.showNextPreview
                    spacing: 4

                    Text {
                        text: "Then"
                        font.pixelSize: ThemeStore.fontBody
                        color: isDark ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(0, 0, 0, 0.6)
                    }
                    Text {
                        text: true
                              ? maneuverIcon(NavigationService.nextManeuverType) : ""
                        font.family: "Material Icons"
                        font.pixelSize: ThemeStore.fontBody
                        color: isDark ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(0, 0, 0, 0.6)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: NavigationService.nextStreetName
                        font.pixelSize: ThemeStore.fontBody
                        color: isDark ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(0, 0, 0, 0.6)
                        elide: Text.ElideRight
                    }
                }
            }

        }

        // Compact Time Info Bar (top-right corner) — floats on top; doesn't affect wrapping
        Rectangle {
            id: timeInfoBar
            z: 1
            anchors.top: parent.top
            anchors.right: parent.right
            implicitWidth: timeRow.width + 16
            implicitHeight: timeRow.height + 8
            color: isDark ? Qt.rgba(0, 0, 0, 0.95) : Qt.rgba(1, 1, 1, 0.98)
            radius: ThemeStore.radiusCard

            // Left and Bottom borders
            Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.12) }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.12) }

            Row {
                id: timeRow
                anchors.centerIn: parent
                spacing: 8

                // Distance remaining
                Row {
                    spacing: 4
                    Text { text: MaterialIcon.iconSpeed; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: true
                              ? formatDistance(NavigationService.distanceToDestination) : ""
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // Time remaining
                Row {
                    spacing: 4
                    Text { text: MaterialIcon.iconTimer; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: NavigationService.remainingDuration > 0
                              ? Math.ceil(NavigationService.remainingDuration / 60) + "m"
                              : ""
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // ETA
                Row {
                    spacing: 4
                    Text { text: MaterialIcon.iconFlag; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: NavigationService.eta
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }
            }
        }
    }
}

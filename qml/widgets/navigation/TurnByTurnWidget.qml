import QtQuick
import QtQuick.Layouts

Item {
    id: tbtWidget
    height: visible ? Math.max(contentCol.height + 16, 96) : 0
    visible: typeof navigationService !== "undefined" && navigationService.isNavigating
             && navigationService.currentManeuverDistance > 0

    property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true

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

    // Material Icons codepoints for navigation
    // Note: These icons are in the Unicode supplementary plane (U+F0XXX).
    // QML's \uXXXX escape only handles 4 hex digits, so we use String.fromCodePoint().
    readonly property string miTurnLeft:        String.fromCodePoint(0xf058f)
    readonly property string miTurnRight:       String.fromCodePoint(0xf0590)
    readonly property string miTurnSharpLeft:   String.fromCodePoint(0xf0591)
    readonly property string miTurnSharpRight:  String.fromCodePoint(0xf0592)
    readonly property string miTurnSlightLeft:  String.fromCodePoint(0xf0593)
    readonly property string miTurnSlightRight: String.fromCodePoint(0xf0594)
    readonly property string miUTurnLeft:       String.fromCodePoint(0xf0595)
    readonly property string miUTurnRight:      String.fromCodePoint(0xf0596)
    readonly property string miStraight:        String.fromCodePoint(0xf0574)
    readonly property string miMerge:           String.fromCodePoint(0xf053b)
    readonly property string miNavigation:      "\ue41e"
    readonly property string miTimer:           "\ue662"
    readonly property string miFlag:            "\ue28e"
    readonly property string miSpeed:           "\ue5e0"

    function maneuverIcon(maneuverType) {
        switch (maneuverType) {
            case mtTurnLeft:                          return miTurnLeft
            case mtTurnSharpLeft:                     return miTurnSharpLeft
            case mtTurnRight:                         return miTurnRight
            case mtTurnSharpRight:                    return miTurnSharpRight
            case mtTurnSlightLeft: case mtKeepLeft:   return miTurnSlightLeft
            case mtTurnSlightRight: case mtKeepRight: return miTurnSlightRight
            case mtUTurn:                             return miUTurnLeft
            case mtUTurnRight:                        return miUTurnRight
            case mtExitLeft:                          return miTurnSlightLeft
            case mtExitRight:                         return miTurnSlightRight
            case mtMergeStraight: case mtMergeLeft: case mtMergeRight:
                                                      return miMerge
            case mtKeepStraight: case mtFerry:        return miStraight
            default:                                  return miStraight
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
            anchors.fill: parent
            spacing: 0

            // Icon box (left-aligned)
            Item {
                Layout.preferredWidth: 80
                Layout.fillHeight: true

                property int mType: typeof navigationService !== "undefined"
                                    ? navigationService.currentManeuverType : 0
                property double mDist: typeof navigationService !== "undefined"
                                       ? navigationService.currentManeuverDistance : 0
                property bool isRoundabout: (mType === mtRoundaboutEnter || mType === mtRoundaboutExit)
                                            && mDist <= iconThreshold(mType)

                Loader {
                    anchors.centerIn: parent
                    active: parent.isRoundabout
                    sourceComponent: RoundaboutIcon {
                        exitNumber: typeof navigationService !== "undefined"
                                    ? Math.max(1, navigationService.roundaboutExitCount) : 1
                        isDark: tbtWidget.isDark
                        size: 64
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: !parent.isRoundabout
                    text: parent.mDist <= iconThreshold(parent.mType)
                          ? maneuverIcon(parent.mType) : miStraight
                    font.family: "Material Icons"
                    font.pixelSize: 64
                    color: isDark ? "white" : "#212121"
                }
            }

            // Text Column (center-expanded)
            ColumnLayout {
                id: contentCol
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 8
                Layout.topMargin: 12
                spacing: 4

                // Distance indicator
                Text {
                    text: typeof navigationService !== "undefined"
                          ? formatDistance(navigationService.currentManeuverDistance) : ""
                    font.pixelSize: 18
                    font.bold: true
                    color: isDark ? "white" : "#212121"
                    lineHeight: 1.0
                }

                // Main instruction text (verbal)
                Text {
                    Layout.fillWidth: true
                    text: typeof navigationService !== "undefined"
                          ? navigationService.currentVerbalInstruction : ""
                    font.pixelSize: 18
                    font.weight: isDark ? Font.Normal : Font.Medium
                    color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    lineHeight: 1.2
                }

                // Next instruction preview
                Row {
                    Layout.fillWidth: true
                    visible: typeof navigationService !== "undefined" && navigationService.showNextPreview
                    spacing: 4

                    Text {
                        text: "Then"
                        font.pixelSize: 14
                        color: isDark ? Qt.rgba(1, 1, 1, 0.38) : Qt.rgba(0, 0, 0, 0.45)
                    }
                    Text {
                        text: typeof navigationService !== "undefined"
                              ? maneuverIcon(navigationService.nextManeuverType) : ""
                        font.family: "Material Icons"
                        font.pixelSize: 14
                        color: isDark ? Qt.rgba(1, 1, 1, 0.38) : Qt.rgba(0, 0, 0, 0.45)
                    }
                    Text {
                        text: typeof navigationService !== "undefined"
                              ? navigationService.nextStreetName : ""
                        font.pixelSize: 14
                        color: isDark ? Qt.rgba(1, 1, 1, 0.38) : Qt.rgba(0, 0, 0, 0.45)
                        elide: Text.ElideRight
                    }
                }
            }

            // Right spacer for Time Info Bar
            Item {
                Layout.preferredWidth: 140
                Layout.fillHeight: true
            }
        }

        // Compact Time Info Bar (top-right corner)
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            implicitWidth: timeRow.width + 16
            implicitHeight: timeRow.height + 8
            color: isDark ? Qt.rgba(0, 0, 0, 0.95) : Qt.rgba(1, 1, 1, 0.98)
            radius: 8

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
                    Text { text: miSpeed; font.family: "Material Icons"; font.pixelSize: 12; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: typeof navigationService !== "undefined"
                              ? formatDistance(navigationService.distanceToDestination) : ""
                        font.pixelSize: 12; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // Time remaining
                Row {
                    spacing: 4
                    Text { text: miTimer; font.family: "Material Icons"; font.pixelSize: 12; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: typeof navigationService !== "undefined" && navigationService.totalDuration > 0
                              ? Math.ceil(navigationService.totalDuration / 60) + "m"
                              : ""
                        font.pixelSize: 12; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // ETA
                Row {
                    spacing: 4
                    Text { text: miFlag; font.family: "Material Icons"; font.pixelSize: 12; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: typeof navigationService !== "undefined" ? navigationService.eta : ""
                        font.pixelSize: 12; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }
            }
        }
    }
}

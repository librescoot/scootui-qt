import QtQuick
import QtQuick.Layouts

Item {
    id: tbtWidget
    height: visible ? contentCol.height + 16 : 0
    visible: typeof navigationService !== "undefined" && navigationService.isNavigating
             && navigationService.currentManeuverDistance > 0

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

    function maneuverArrow(maneuverType) {
        switch (maneuverType) {
            case mtTurnLeft: case mtTurnSharpLeft: return "\u2190"      // ←
            case mtTurnRight: case mtTurnSharpRight: return "\u2192"    // →
            case mtTurnSlightLeft: case mtKeepLeft: return "\u2196"     // ↖
            case mtTurnSlightRight: case mtKeepRight: return "\u2197"   // ↗
            case mtUTurn: case mtUTurnRight: return "\u21B6"            // ↶
            case mtRoundaboutEnter: case mtRoundaboutExit: return "\u21BB" // ↻
            case mtExitLeft: return "\u21B0"                             // ↰
            case mtExitRight: return "\u21B1"                            // ↱
            case mtMergeStraight: case mtMergeLeft: case mtMergeRight:
            case mtKeepStraight: return "\u2191"                        // ↑
            case mtFerry: return "\u26F4"                                // ⛴
            default: return "\u2191"                                     // ↑
        }
    }

    // Main background container
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.8) // Matching Flutter's 0.8 opacity

        // Bottom border matching Flutter
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.1)
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
                        isDark: true
                        size: 64
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: !parent.isRoundabout
                    text: parent.mDist <= iconThreshold(parent.mType)
                          ? maneuverArrow(parent.mType) : "\u2191"
                    font.pixelSize: 64
                    font.bold: true
                    color: "white"
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
                    color: "white"
                    lineHeight: 1.0
                }

                // Main instruction text (verbal)
                Text {
                    Layout.fillWidth: true
                    text: typeof navigationService !== "undefined"
                          ? navigationService.currentVerbalInstruction : ""
                    font.pixelSize: 18
                    font.weight: Font.Normal
                    color: Qt.rgba(1, 1, 1, 0.7)
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    lineHeight: 1.2
                }

                // Next instruction preview
                Text {
                    Layout.fillWidth: true
                    visible: typeof navigationService !== "undefined" && navigationService.showNextPreview
                    text: {
                        if (typeof navigationService === "undefined") return ""
                        var arrow = maneuverArrow(navigationService.nextManeuverType)
                        return "Then " + arrow + " " + navigationService.nextStreetName
                    }
                    font.pixelSize: 14
                    color: Qt.rgba(1, 1, 1, 0.38)
                    elide: Text.ElideRight
                    lineHeight: 1.2
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
            color: Qt.rgba(0, 0, 0, 0.95)
            radius: 8 // Corner radius for the floating bar
            // Only rounded on bottom-left to match Flutter's style if preferred,
            // but here we match the "box in corner" look.

            // Left and Bottom borders
            Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: Qt.rgba(1, 1, 1, 0.1) }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.1) }

            Row {
                id: timeRow
                anchors.centerIn: parent
                spacing: 8

                // Distance remaining
                Row {
                    spacing: 4
                    Text { text: "\u219D"; font.pixelSize: 12; color: Qt.rgba(1, 1, 1, 0.54) } // Straighten icon approx
                    Text {
                        text: typeof navigationService !== "undefined"
                              ? formatDistance(navigationService.distanceToDestination) : ""
                        font.pixelSize: 12; color: Qt.rgba(1, 1, 1, 0.7)
                    }
                }

                // Time remaining
                Row {
                    spacing: 4
                    Text { text: "\u23F2"; font.pixelSize: 12; color: Qt.rgba(1, 1, 1, 0.54) } // Timer icon
                    Text {
                        text: typeof navigationService !== "undefined" && navigationService.totalDuration > 0
                              ? Math.ceil(navigationService.totalDuration / 60) + "m"
                              : ""
                        font.pixelSize: 12; color: Qt.rgba(1, 1, 1, 0.7)
                    }
                }

                // ETA
                Row {
                    spacing: 4
                    Text { text: "\u2691"; font.pixelSize: 12; color: Qt.rgba(1, 1, 1, 0.54) } // Flag icon
                    Text {
                        text: typeof navigationService !== "undefined" ? navigationService.eta : ""
                        font.pixelSize: 12; color: Qt.rgba(1, 1, 1, 0.7)
                    }
                }
            }
        }
    }
}

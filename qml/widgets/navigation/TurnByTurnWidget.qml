import QtQuick
import QtQuick.Layouts

Rectangle {
    id: tbtWidget
    color: Qt.rgba(0, 0, 0, 0.85)
    radius: 8
    visible: typeof navigationService !== "undefined" && navigationService.isNavigating
             && navigationService.currentManeuverDistance > 0

    implicitHeight: contentCol.height + 16

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

    // Distance threshold for showing maneuver icon vs straight arrow
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

    ColumnLayout {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 4

        // Main instruction row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // Maneuver icon — use RoundaboutIcon canvas for roundabout types,
            // otherwise fall back to the Unicode arrow text.
            Item {
                id: maneuverIconContainer
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56

                property int mType: typeof navigationService !== "undefined"
                                    ? navigationService.currentManeuverType : 0
                property double mDist: typeof navigationService !== "undefined"
                                       ? navigationService.currentManeuverDistance : 0
                property bool isRoundabout: (mType === mtRoundaboutEnter || mType === mtRoundaboutExit)
                                            && mDist <= iconThreshold(mType)

                Loader {
                    id: roundaboutLoader
                    anchors.centerIn: parent
                    active: maneuverIconContainer.isRoundabout
                    sourceComponent: RoundaboutIcon {
                        exitNumber: typeof navigationService !== "undefined"
                                    ? Math.max(1, navigationService.roundaboutExitCount) : 1
                        isDark: true
                        size: 48
                    }
                }

                Text {
                    id: maneuverIcon
                    anchors.centerIn: parent
                    visible: !maneuverIconContainer.isRoundabout
                    text: maneuverIconContainer.mDist <= iconThreshold(maneuverIconContainer.mType)
                          ? maneuverArrow(maneuverIconContainer.mType) : "\u2191"
                    font.pixelSize: 48
                    font.bold: true
                    color: "white"
                }
            }

            // Distance + street
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: typeof navigationService !== "undefined"
                          ? formatDistance(navigationService.currentManeuverDistance) : ""
                    font.pixelSize: 28
                    font.bold: true
                    color: "white"
                }

                Text {
                    Layout.fillWidth: true
                    text: typeof navigationService !== "undefined"
                          ? navigationService.currentStreetName : ""
                    font.pixelSize: 14
                    color: Qt.rgba(1, 1, 1, 0.7)
                    elide: Text.ElideRight
                    visible: text.length > 0
                }
            }
        }

        // Verbal instruction
        Text {
            Layout.fillWidth: true
            text: typeof navigationService !== "undefined"
                  ? navigationService.currentVerbalInstruction : ""
            font.pixelSize: 13
            color: Qt.rgba(1, 1, 1, 0.6)
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            visible: text.length > 0
        }

        // Next instruction preview
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: nextRow.height + 8
            color: Qt.rgba(1, 1, 1, 0.1)
            radius: 4
            visible: typeof navigationService !== "undefined" && navigationService.showNextPreview

            RowLayout {
                id: nextRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 6
                spacing: 8

                Text {
                    text: "Then"
                    font.pixelSize: 12
                    color: Qt.rgba(1, 1, 1, 0.5)
                }

                Text {
                    text: typeof navigationService !== "undefined"
                          ? maneuverArrow(navigationService.nextManeuverType) : ""
                    font.pixelSize: 20
                    font.bold: true
                    color: Qt.rgba(1, 1, 1, 0.8)
                }

                Text {
                    Layout.fillWidth: true
                    text: typeof navigationService !== "undefined"
                          ? navigationService.nextStreetName : ""
                    font.pixelSize: 12
                    color: Qt.rgba(1, 1, 1, 0.6)
                    elide: Text.ElideRight
                }

                Text {
                    text: typeof navigationService !== "undefined"
                          ? formatDistance(navigationService.nextManeuverDistance) : ""
                    font.pixelSize: 12
                    color: Qt.rgba(1, 1, 1, 0.5)
                }
            }
        }

        // Time info bar (distance remaining, time remaining, ETA)
        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: typeof navigationService !== "undefined"
                      ? formatDistance(navigationService.distanceToDestination) : ""
                font.pixelSize: 11
                color: Qt.rgba(1, 1, 1, 0.5)
            }

            Text {
                text: typeof navigationService !== "undefined" && navigationService.totalDuration > 0
                      ? Math.ceil(navigationService.totalDuration / 60) + " min"
                      : ""
                font.pixelSize: 11
                color: Qt.rgba(1, 1, 1, 0.5)
            }

            Text {
                Layout.leftMargin: 12
                text: typeof navigationService !== "undefined"
                      ? "ETA " + navigationService.eta : ""
                font.pixelSize: 11
                color: Qt.rgba(1, 1, 1, 0.5)
                visible: text.length > 4
            }
        }
    }
}

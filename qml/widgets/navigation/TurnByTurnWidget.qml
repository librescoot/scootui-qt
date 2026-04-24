import QtQuick
import QtQuick.Layouts
import "../components"

Item {
    id: tbtWidget
    // Natural size from content, floored at 96 for a stable idle footprint.
    // Instruction text self-caps at maximumLineCount so the floor never gets
    // overshot by a runaway string. Publishing as implicitHeight lets Layout
    // containers read the widget's size without an external binding.
    implicitHeight: Math.max(contentCol.implicitHeight + 24, 96)
    // Show whenever we have any upcoming maneuver. The previous gate hid the
    // banner at distance=0 for regular turns (only kStart/Arrive were kept
    // visible there), which blanked the banner at the exact shape index of
    // every turn — precisely when the rider needs the "execute now" cue.
    visible: typeof navigationService !== "undefined" && navigationService.isNavigating
             && navigationService.hasCurrentManeuver

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
    readonly property int mtArrive: 20
    readonly property int mtArriveRight: 21
    readonly property int mtArriveLeft: 22

    function iconThreshold(maneuverType) {
        switch (maneuverType) {
            case mtUTurn: case mtUTurnRight: return 600
            case mtRoundaboutEnter: case mtRoundaboutExit:
            case mtTurnSharpLeft: case mtTurnSharpRight: return 500
            case mtArrive: case mtArriveRight: case mtArriveLeft: return 500
            case mtTurnLeft: case mtTurnRight:
            case mtExitLeft: case mtExitRight: return 400
            case mtTurnSlightLeft: case mtTurnSlightRight:
            case mtMergeStraight: case mtMergeLeft: case mtMergeRight: return 300
            // Fork-style lane pick — needs lane-shift time, more than a
            // gentle bend. ~18 s at 50 km/h to spot the fork and reposition.
            case mtKeepLeft: case mtKeepRight: return 250
            case mtKeepStraight: return 150
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
            case mtTurnSlightLeft:                    return MaterialIcon.iconTurnSlightLeft
            case mtTurnSlightRight:                   return MaterialIcon.iconTurnSlightRight
            // Keep* is a fork/lane pick, not a turn. Fork arrow reads as
            // "split in the road" rather than "bend a bit".
            case mtKeepLeft:                          return MaterialIcon.iconForkLeft
            case mtKeepRight:                         return MaterialIcon.iconForkRight
            case mtUTurn:                             return MaterialIcon.iconUTurnLeft
            case mtUTurnRight:                        return MaterialIcon.iconUTurnRight
            case mtExitLeft:                          return MaterialIcon.iconTurnSlightLeft
            case mtExitRight:                         return MaterialIcon.iconTurnSlightRight
            case mtMergeStraight: case mtMergeLeft: case mtMergeRight:
                                                      return MaterialIcon.iconMerge
            case mtArrive: case mtArriveRight: case mtArriveLeft:
                                                      return MaterialIcon.iconFlag
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
                          ? maneuverIcon(parent.mType) : MaterialIcon.iconStraight
                    font.family: "Material Icons"
                    font.pixelSize: themeStore.fontHero
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

                // Distance indicator. Hidden for kStart-family ("head on X")
                // because the rider is AT the start and 0 m is noise.
                Text {
                    Layout.fillWidth: true
                    visible: typeof navigationService === "undefined"
                             || !navigationService.currentIsStart
                    text: typeof navigationService !== "undefined"
                          ? formatDistance(navigationService.currentManeuverDistance) : ""
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Bold
                    color: isDark ? "white" : "#212121"
                    lineHeight: 1.0
                }

                // Main instruction text (verbal) — wraps up to 3 lines so a
                // runaway instruction can't blow out the banner height.
                Text {
                    Layout.fillWidth: true
                    text: typeof navigationService !== "undefined"
                          ? navigationService.currentVerbalInstruction : ""
                    font.pixelSize: themeStore.fontBody
                    font.weight: isDark ? Font.Normal : Font.Medium
                    color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    lineHeight: 1.2
                }

                // Next instruction preview
                RowLayout {
                    Layout.fillWidth: true
                    visible: typeof navigationService !== "undefined" && navigationService.showNextPreview
                    spacing: 4

                    Text {
                        text: "Then"
                        font.pixelSize: themeStore.fontBody
                        color: isDark ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(0, 0, 0, 0.6)
                    }
                    Text {
                        text: typeof navigationService !== "undefined"
                              ? maneuverIcon(navigationService.nextManeuverType) : ""
                        font.family: "Material Icons"
                        font.pixelSize: themeStore.fontBody
                        color: isDark ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(0, 0, 0, 0.6)
                    }
                    Text {
                        Layout.fillWidth: true
                        // Arrive maneuvers have no street name — Valhalla
                        // emits "You have arrived" as the instruction, not
                        // a street. Fall back to "arrive" so the preview
                        // reads "Then [flag] arrive" rather than a bare flag.
                        text: {
                            if (typeof navigationService === "undefined") return ""
                            var nt = navigationService.nextManeuverType
                            var name = navigationService.nextStreetName
                            var isArrive = (nt === mtArrive || nt === mtArriveRight || nt === mtArriveLeft)
                            if (name && name.length > 0) return name
                            return isArrive ? "arrive" : ""
                        }
                        font.pixelSize: themeStore.fontBody
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
            radius: themeStore.radiusCard

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
                        text: typeof navigationService !== "undefined"
                              ? formatDistance(navigationService.distanceToDestination) : ""
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // Time remaining
                Row {
                    spacing: 4
                    Text { text: MaterialIcon.iconTimer; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: typeof navigationService !== "undefined" && navigationService.remainingDuration > 0
                              ? Math.ceil(navigationService.remainingDuration / 60) + "m"
                              : ""
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // ETA
                Row {
                    spacing: 4
                    Text { text: MaterialIcon.iconFlag; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: typeof navigationService !== "undefined" ? navigationService.eta : ""
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }
            }
        }
    }
}

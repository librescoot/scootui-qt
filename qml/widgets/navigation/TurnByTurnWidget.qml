import QtQuick
import QtQuick.Layouts
import ScootUI 1.0
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

    // Guarded against null as well as undefined: ClusterScreen incubates this
    // through an asynchronous Loader, and `typeof null` is "object", so the
    // typeof check alone lets a null context property through to the read.
    property bool isDark: (typeof themeStore !== "undefined" && themeStore)
                          ? themeStore.isDark : true


    function iconThreshold(maneuverType) {
        switch (maneuverType) {
            case Scooter.ManeuverType.UTurn: case Scooter.ManeuverType.UTurnRight: return 600
            case Scooter.ManeuverType.RoundaboutEnter: case Scooter.ManeuverType.RoundaboutExit:
            case Scooter.ManeuverType.TurnSharpLeft: case Scooter.ManeuverType.TurnSharpRight: return 500
            case Scooter.ManeuverType.Arrive: case Scooter.ManeuverType.ArriveRight: case Scooter.ManeuverType.ArriveLeft: return 500
            case Scooter.ManeuverType.TurnLeft: case Scooter.ManeuverType.TurnRight:
            case Scooter.ManeuverType.ExitLeft: case Scooter.ManeuverType.ExitRight: return 400
            case Scooter.ManeuverType.TurnSlightLeft: case Scooter.ManeuverType.TurnSlightRight:
            case Scooter.ManeuverType.MergeStraight: case Scooter.ManeuverType.MergeLeft: case Scooter.ManeuverType.MergeRight: return 300
            // Fork-style lane pick — needs lane-shift time, more than a
            // gentle bend. ~18 s at 50 km/h to spot the fork and reposition.
            case Scooter.ManeuverType.KeepLeft: case Scooter.ManeuverType.KeepRight: return 250
            case Scooter.ManeuverType.KeepStraight: return 150
            default: return 1000
        }
    }

    // Thin space (U+2009) between value and unit per typographic convention.
    function formatDistance(meters) {
        if (meters >= 1000) return (meters / 1000).toFixed(1) + " km"
        if (meters >= 100) return (Math.round(meters / 100) * 100) + " m"
        if (meters >= 10) return (Math.round(meters / 10) * 10) + " m"
        return Math.round(meters) + " m"
    }

    // Human-friendly remaining trip time. "min" — not bare "m" which reads
    // as SI metres. For ≥1 h, splits into "X h Y min"; "Y min" is dropped
    // when zero so a clean hour reads as "1 h", not "1 h 0 min".
    function formatRemainingTime(seconds) {
        if (seconds <= 0) return ""
        var totalMin = Math.ceil(seconds / 60)
        if (totalMin < 60) return totalMin + " min"
        var h = Math.floor(totalMin / 60)
        var m = totalMin % 60
        if (m === 0) return h + " h"
        return h + " h " + m + " min"
    }

    function maneuverIcon(maneuverType) {
        switch (maneuverType) {
            case Scooter.ManeuverType.TurnLeft:                          return MaterialIcon.iconTurnLeft
            case Scooter.ManeuverType.TurnSharpLeft:                     return MaterialIcon.iconTurnSharpLeft
            case Scooter.ManeuverType.TurnRight:                         return MaterialIcon.iconTurnRight
            case Scooter.ManeuverType.TurnSharpRight:                    return MaterialIcon.iconTurnSharpRight
            case Scooter.ManeuverType.TurnSlightLeft:                    return MaterialIcon.iconTurnSlightLeft
            case Scooter.ManeuverType.TurnSlightRight:                   return MaterialIcon.iconTurnSlightRight
            // Keep* main icon is rendered as a two-tone SVG (see Image block
            // below); this Material glyph is only used for the small inline
            // next-preview, where two-tone dimming wouldn't be readable
            // anyway. Slight-turn reads cleanly at preview size.
            case Scooter.ManeuverType.KeepLeft:                          return MaterialIcon.iconTurnSlightLeft
            case Scooter.ManeuverType.KeepRight:                         return MaterialIcon.iconTurnSlightRight
            case Scooter.ManeuverType.UTurn:                             return MaterialIcon.iconUTurnLeft
            case Scooter.ManeuverType.UTurnRight:                        return MaterialIcon.iconUTurnRight
            case Scooter.ManeuverType.ExitLeft:                          return MaterialIcon.iconTurnSlightLeft
            case Scooter.ManeuverType.ExitRight:                         return MaterialIcon.iconTurnSlightRight
            case Scooter.ManeuverType.MergeStraight: case Scooter.ManeuverType.MergeLeft: case Scooter.ManeuverType.MergeRight:
                                                      return MaterialIcon.iconMerge
            case Scooter.ManeuverType.Arrive: case Scooter.ManeuverType.ArriveRight: case Scooter.ManeuverType.ArriveLeft:
                                                      return MaterialIcon.iconFlag
            case Scooter.ManeuverType.KeepStraight: case Scooter.ManeuverType.Ferry:        return MaterialIcon.iconStraight
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
                property bool isRoundabout: (mType === Scooter.ManeuverType.RoundaboutEnter || mType === Scooter.ManeuverType.RoundaboutExit)
                                            && mDist <= iconThreshold(mType)
                // Keep L/R uses two-tone SVGs (active arm bright, inactive arm
                // dimmed) so the rider sees which fork to take, not just that
                // there is one. Only kicks in within the announce threshold —
                // outside it the Text fallback shows a plain straight arrow.
                property bool isKeepFork: (mType === Scooter.ManeuverType.KeepLeft || mType === Scooter.ManeuverType.KeepRight)
                                          && mDist <= iconThreshold(mType)

                Loader {
                    anchors.centerIn: parent
                    active: parent.isRoundabout
                    sourceComponent: RoundaboutIconFromMap {
                        renderData: typeof navigationService !== "undefined"
                                    ? navigationService.currentRoundaboutRender : null
                        isDark: tbtWidget.isDark
                        size: 80
                    }
                }

                Image {
                    anchors.centerIn: parent
                    visible: parent.isKeepFork
                    width: 64
                    height: 64
                    sourceSize.width: 64
                    sourceSize.height: 64
                    fillMode: Image.PreserveAspectFit
                    source: {
                        if (parent.mType === Scooter.ManeuverType.KeepLeft)
                            return isDark ? "qrc:/ScootUI/assets/icons/librescoot-keep-left.svg"
                                          : "qrc:/ScootUI/assets/icons/librescoot-keep-left-light.svg"
                        if (parent.mType === Scooter.ManeuverType.KeepRight)
                            return isDark ? "qrc:/ScootUI/assets/icons/librescoot-keep-right.svg"
                                          : "qrc:/ScootUI/assets/icons/librescoot-keep-right-light.svg"
                        return ""
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: !parent.isRoundabout && !parent.isKeepFork
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
                            var isArrive = (nt === Scooter.ManeuverType.Arrive || nt === Scooter.ManeuverType.ArriveRight || nt === Scooter.ManeuverType.ArriveLeft)
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
                // Larger gap between the three metric groups so the eye reads
                // them as separate. Inner icon→value gap stays tight (2 px).
                spacing: 14

                // Distance remaining
                Row {
                    spacing: 2
                    Text { text: MaterialIcon.iconSpeed; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: typeof navigationService !== "undefined"
                              ? formatDistance(navigationService.distanceToDestination) : ""
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // Time remaining
                Row {
                    spacing: 2
                    Text { text: MaterialIcon.iconTimer; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: typeof navigationService !== "undefined"
                              ? formatRemainingTime(navigationService.remainingDuration) : ""
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // ETA
                Row {
                    spacing: 2
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

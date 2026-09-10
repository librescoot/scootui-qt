import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../../notifications"
import "../components"

Item {
    id: tbtWidget
    property var maneuver: ({})
    property bool compact: false
    property bool paired: false
    property var queuedCounts: ({})
    readonly property bool arrived: maneuver.status === 4
    readonly property bool navigating: maneuver.status === 2 || arrived
    readonly property bool loading: maneuver.status === 1 || maneuver.status === 3
    // A start instruction has no distance row, so leave room for the trip summary above it.
    implicitHeight: compact ? Math.max(contentCol.implicitHeight + 12, 48)
                   : navigating ? Math.max(contentCol.implicitHeight + (paired ? 12 : 24)
                                          + (maneuver.isStart && timeInfoBar.visible ? timeInfoBar.height : 0), counts.height, 96) : Math.max(counts.height, 96)

    property bool isDark: (typeof themeStore !== "undefined" && themeStore)
                          ? themeStore.isDark : true

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

    function arrivalInstruction() {
        if (maneuver.maneuverType === mtArriveRight)
            return typeof translations !== "undefined" ? translations.navDestinationRight : "Your destination is on your right"
        if (maneuver.maneuverType === mtArriveLeft)
            return typeof translations !== "undefined" ? translations.navDestinationLeft : "Your destination is on your left"
        return typeof translations !== "undefined" ? translations.navDestinationHere : "Your destination is here"
    }

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
            case mtTurnLeft:                          return MaterialIcon.iconTurnLeft
            case mtTurnSharpLeft:                     return MaterialIcon.iconTurnSharpLeft
            case mtTurnRight:                         return MaterialIcon.iconTurnRight
            case mtTurnSharpRight:                    return MaterialIcon.iconTurnSharpRight
            case mtTurnSlightLeft:                    return MaterialIcon.iconTurnSlightLeft
            case mtTurnSlightRight:                   return MaterialIcon.iconTurnSlightRight
            // Keep* main icon is rendered as a two-tone SVG (see Image block
            // below); this Material glyph is only used for the small inline
            // next-preview, where two-tone dimming wouldn't be readable
            // anyway. Slight-turn reads cleanly at preview size.
            case mtKeepLeft:                          return MaterialIcon.iconTurnSlightLeft
            case mtKeepRight:                         return MaterialIcon.iconTurnSlightRight
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

    QueuedNotificationCounts {
        id: counts
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 8
        queuedCounts: tbtWidget.queuedCounts
        isDark: tbtWidget.isDark
        z: 2
    }

    // Main background container
    Rectangle {
        objectName: "maneuverBackground"
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
            visible: tbtWidget.navigating || tbtWidget.loading
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: counts.hasCounts ? counts.width + 8 : 0
            spacing: 0

            // Icon box (left-aligned)
            Item {
                Layout.preferredWidth: tbtWidget.compact ? 48 : 80
                Layout.preferredHeight: tbtWidget.compact ? 48 : 80

                objectName: "maneuverIconBox"
                property int mType: tbtWidget.maneuver.maneuverType || 0
                property double mDist: tbtWidget.maneuver.distance || 0
                property bool isRoundabout: !tbtWidget.loading && (mType === mtRoundaboutEnter || mType === mtRoundaboutExit)
                                            && mDist <= iconThreshold(mType)
                // Keep L/R uses two-tone SVGs (active arm bright, inactive arm
                // dimmed) so the rider sees which fork to take, not just that
                // there is one. Only kicks in within the announce threshold —
                // outside it the Text fallback shows a plain straight arrow.
                property bool isKeepFork: !tbtWidget.loading && (mType === mtKeepLeft || mType === mtKeepRight)
                                          && mDist <= iconThreshold(mType)

                Loader {
                    anchors.centerIn: parent
                    // The warmed hidden cluster must not compete with the visible
                    // map screen for street-query demand. Service prefetch is independent.
                    active: parent.isRoundabout && tbtWidget.visible
                    sourceComponent: RoundaboutIconFromMap {
                        objectName: "mainRoundaboutIcon"
                        renderData: tbtWidget.maneuver.roundabout || null
                        fallbackExitNumber: Math.max(1, tbtWidget.maneuver.roundaboutExit || 0)
                        isDark: tbtWidget.isDark
                        size: tbtWidget.compact ? 40 : 80
                    }
                }

                Image {
                    objectName: "maneuverForkIcon"
                    anchors.centerIn: parent
                    visible: parent.isKeepFork
                    width: tbtWidget.compact ? 32 : 64
                    height: width
                    sourceSize.width: width
                    sourceSize.height: height
                    fillMode: Image.PreserveAspectFit
                    source: {
                        if (parent.mType === mtKeepLeft)
                            return isDark ? "qrc:/ScootUI/assets/icons/librescoot-keep-left.svg"
                                          : "qrc:/ScootUI/assets/icons/librescoot-keep-left-light.svg"
                        if (parent.mType === mtKeepRight)
                            return isDark ? "qrc:/ScootUI/assets/icons/librescoot-keep-right.svg"
                                          : "qrc:/ScootUI/assets/icons/librescoot-keep-right-light.svg"
                        return ""
                    }
                }

                BusyIndicator {
                    objectName: "navigationLoadingIndicator"
                    anchors.centerIn: parent
                    width: tbtWidget.compact ? 46 : 76
                    height: width
                    palette.text: tbtWidget.isDark ? "white" : "#212121"
                    palette.dark: tbtWidget.isDark ? "white" : "#212121"
                    running: tbtWidget.loading
                    visible: running
                }

                Text {
                    objectName: "maneuverGlyph"
                    anchors.centerIn: parent
                    visible: !parent.isRoundabout && !parent.isKeepFork
                    text: tbtWidget.loading ? MaterialIcon.iconNavigation
                          : parent.mDist <= iconThreshold(parent.mType)
                          ? maneuverIcon(parent.mType) : MaterialIcon.iconStraight
                    font.family: "Material Icons"
                    font.pixelSize: tbtWidget.loading ? 28 : tbtWidget.compact ? 32 : themeStore.fontHero
                    color: isDark ? "white" : "#212121"
                }
            }

            // Text Column (center-expanded)
            GridLayout {
                id: contentCol
                columns: tbtWidget.compact ? 2 : 1
                rowSpacing: 4
                columnSpacing: 8
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.topMargin: tbtWidget.compact || tbtWidget.paired ? 6 : 12 + (tbtWidget.maneuver.isStart && timeInfoBar.visible ? timeInfoBar.height : 0)
                Layout.bottomMargin: tbtWidget.compact || tbtWidget.paired ? 6 : 8

                // Distance indicator. Hidden for kStart-family ("head on X")
                // because the rider is AT the start and 0 m is noise.
                Text {
                    Layout.fillWidth: !tbtWidget.compact
                    objectName: "maneuverDistance"
                    Layout.alignment: tbtWidget.compact ? Qt.AlignBaseline : Qt.AlignVCenter
                    Layout.rightMargin: timeInfoBar.visible ? timeInfoBar.width : 0
                    visible: tbtWidget.navigating && !tbtWidget.arrived && !tbtWidget.maneuver.isStart
                    text: formatDistance(tbtWidget.maneuver.distance || 0)
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Bold
                    color: isDark ? "white" : "#212121"
                    lineHeight: 1.0
                }

                // Main instruction text (verbal) — wraps up to 3 lines so a
                // runaway instruction can't blow out the banner height.
                Text {
                    Layout.fillWidth: true
                    objectName: "maneuverInstruction"
                    textFormat: Text.PlainText
                    Layout.alignment: tbtWidget.compact ? Qt.AlignBaseline : Qt.AlignVCenter
                    text: tbtWidget.loading
                          ? (maneuver.status === 1
                             ? (typeof translations !== "undefined" ? translations.navCalculating : "Calculating route…")
                             : (typeof translations !== "undefined" ? translations.navRecalculating : "Recalculating route…"))
                          : tbtWidget.arrived ? arrivalInstruction()
                          : (tbtWidget.compact && tbtWidget.maneuver.compactInstruction)
                          || tbtWidget.maneuver.instruction || tbtWidget.maneuver.street || "Navigation"
                    font.pixelSize: themeStore.fontBody
                    font.weight: isDark ? Font.Normal : Font.Medium
                    color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    wrapMode: Text.WordWrap
                    maximumLineCount: tbtWidget.compact ? 1 : tbtWidget.paired ? 2 : 3
                    elide: Text.ElideRight
                    lineHeight: 1.2
                }

                // Next instruction preview
                RowLayout {
                    Layout.fillWidth: true
                    objectName: "maneuverNextPreview"
                    visible: tbtWidget.navigating && !tbtWidget.arrived && !tbtWidget.compact && !tbtWidget.paired && !!tbtWidget.maneuver.showNextPreview
                    spacing: 4

                    Text {
                        // Same string as the spoken-instruction prefix.
                        text: typeof translations !== "undefined" ? translations.navThen : "Then"
                        font.pixelSize: themeStore.fontBody
                        color: isDark ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(0, 0, 0, 0.6)
                    }
                    Text {
                        text: maneuverIcon(tbtWidget.maneuver.nextType || 0)
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
                            var nt = tbtWidget.maneuver.nextType
                            var name = tbtWidget.maneuver.nextStreet
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

        // Compact trip summary, above the instruction.
        Rectangle {
            id: timeInfoBar
            objectName: "maneuverTripSummary"
            visible: tbtWidget.navigating && !tbtWidget.arrived && !tbtWidget.compact && !tbtWidget.paired
            z: 1
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.rightMargin: counts.hasCounts ? counts.width + 16 : 0
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
                        text: formatDistance(tbtWidget.maneuver.distanceToDestination || 0)
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // Time remaining
                Row {
                    spacing: 2
                    Text { text: MaterialIcon.iconTimer; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: formatRemainingTime(tbtWidget.maneuver.remainingDuration || 0)
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }

                // ETA
                Row {
                    spacing: 2
                    Text { text: MaterialIcon.iconFlag; font.family: "Material Icons"; font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.54) : Qt.rgba(0, 0, 0, 0.54) }
                    Text {
                        text: tbtWidget.maneuver.eta || ""
                        font.pixelSize: 13; color: isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    }
                }
            }
        }
    }
}

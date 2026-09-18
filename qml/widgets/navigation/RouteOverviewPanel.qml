import QtQuick
import QtQuick.Layouts
import "../components"

// Route overview for a multi-hop plan: one row per remaining hop with its own
// time and distance, so "A, 17 min, 3 km, B, 27 min, 9.5 km, C" is readable at
// a glance. Data comes from NavigationService::planOverview, which is filled by
// one multi-stop preview request and re-based on every hop change.
Rectangle {
    id: card

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#B3FFFFFF" : "#B3000000"
    readonly property color rowBg: isDark ? "#14FFFFFF" : "#0F000000"

    readonly property var hops: typeof navigationService !== "undefined"
                                ? navigationService.planOverview : []
    readonly property bool hasNavService: typeof navigationService !== "undefined"
    readonly property int currentStep: hasNavService ? navigationService.currentStep : -1
    readonly property bool overviewVisible: typeof mapService !== "undefined"
                                            && mapService.routeOverviewActive === true
                                            && hasNavService
                                            && navigationService.hasPlan === true
                                            && hops.length > 0

    visible: overviewVisible
    implicitHeight: overviewVisible ? content.implicitHeight + 24 : 0
    radius: themeStore.radiusCard
    color: isDark ? "#D9000000" : "#D9FFFFFF"
    border.width: 1
    border.color: isDark ? "#33FFFFFF" : "#1F000000"

    function formatDistance(meters) {
        if (!meters || meters <= 0)
            return ""
        if (meters >= 1000)
            return (meters / 1000).toFixed(meters >= 10000 ? 0 : 1) + " km"
        return Math.round(meters) + " m"
    }

    function formatDuration(seconds) {
        if (!seconds || seconds <= 0)
            return ""
        var minutes = Math.round(seconds / 60)
        if (minutes < 1)
            minutes = 1
        if (minutes < 60)
            return minutes + " min"
        var hours = Math.floor(minutes / 60)
        var rest = minutes % 60
        return rest > 0 ? hours + " h " + rest + " min" : hours + " h"
    }

    function legMetric(hop) {
        if (!hop.ready)
            return (typeof translations !== "undefined" && translations.routeOverviewCalculating)
                   ? translations.routeOverviewCalculating : ""
        var distance = formatDistance(hop.distance)
        var duration = formatDuration(hop.duration)
        if (distance === "")
            return duration
        if (duration === "")
            return distance
        return duration + " · " + distance
    }

    function rowLabel(hop) {
        return hop.toLabel !== "" ? hop.toLabel
                                  : hop.toIndex + 1
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: (typeof translations !== "undefined" && translations.routeOverviewTitle)
                      ? translations.routeOverviewTitle : ""
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
                color: card.textPrimary
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: card.hasNavService && navigationService.planTotalDuration > 0
                text: (typeof translations !== "undefined" && translations.routeOverviewTotal)
                      ? translations.routeOverviewTotal
                            .arg(card.formatDuration(navigationService.planTotalDuration))
                            .arg(card.formatDistance(navigationService.planTotalDistance))
                      : ""
                font.pixelSize: themeStore.fontCaption
                color: card.textSecondary
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 200)
            clip: true
            interactive: contentHeight > height
            model: card.hops

            delegate: Rectangle {
                required property var modelData
                width: list.width
                height: 30
                radius: themeStore.radiusCard
                color: card.rowBg

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Rectangle {
                        width: 18
                        height: 18
                        radius: 9
                        color: (modelData.toIndex === card.currentStep)
                               ? themeStore.statusSuccess : card.rowBg
                        Text {
                            anchors.centerIn: parent
                            text: modelData.toIndex + 1
                            font.pixelSize: themeStore.fontCaption
                            color: card.textPrimary
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: card.rowLabel(modelData)
                        font.pixelSize: themeStore.fontBody
                        color: card.textPrimary
                        elide: Text.ElideRight
                    }

                    Text {
                        text: card.legMetric(modelData)
                        font.pixelSize: themeStore.fontCaption
                        color: card.textSecondary
                    }
                }
            }
        }
    }
}

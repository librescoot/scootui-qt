import QtQuick
import QtQuick.Layouts
import "../components"

// Extent-aware route overview with totals and optional remaining-hop list.
Item {
    id: panel

    readonly property var hops: typeof navigationService !== "undefined"
                                ? navigationService.planOverview : []
    readonly property bool overviewVisible: typeof mapService !== "undefined"
                                            && mapService.routeOverviewActive === true
                                            && typeof navigationService !== "undefined"
                                            && navigationService.hasRoute === true
    readonly property bool multiHop: typeof navigationService !== "undefined"
                                     && navigationService.hasPlan === true
                                     && navigationService.stopCount >= 2
                                     && hops.length > 0
    readonly property real totalDistance: multiHop ? navigationService.planTotalDistance
                                                    : navigationService.totalDistance
    readonly property real totalDuration: multiHop ? navigationService.planTotalDuration
                                                    : navigationService.totalDuration
    readonly property int currentStep: typeof navigationService !== "undefined"
                                       ? navigationService.currentStep : -1

    visible: overviewVisible
    height: card.implicitHeight

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

    function stopLabel(hop) {
        if (hop.toLabel !== "")
            return hop.toLabel
        return String(hop.toIndex + 1)
    }

    Rectangle {
        id: card
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        implicitHeight: content.implicitHeight + 20
        radius: themeStore.radiusCard
        color: Qt.rgba(themeStore.surfaceColor.r, themeStore.surfaceColor.g,
                       themeStore.surfaceColor.b, 0.92)
        border.width: 1
        border.color: themeStore.borderColor

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
                    font.weight: Font.Medium
                    color: themeStore.textColor
                }
                Item { Layout.fillWidth: true }
                Text {
                    visible: panel.totalDistance > 0 || panel.totalDuration > 0
                    text: (typeof translations !== "undefined" && translations.routeOverviewTotal)
                          ? translations.routeOverviewTotal
                                .arg(panel.formatDuration(panel.totalDuration))
                                .arg(panel.formatDistance(panel.totalDistance))
                          : ""
                    font.pixelSize: themeStore.fontCaption
                    color: themeStore.textSecondary
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Repeater {
                    model: [
                        { label: translations.routeOverviewStart, color: "#2E7D32" },
                        { label: translations.routeOverviewFinish, color: "#C62828" },
                        { label: translations.routeOverviewYou, color: mapService.routeFillColor }
                    ]
                    delegate: RowLayout {
                        required property var modelData
                        spacing: 4
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: modelData.color
                            border.color: "white"
                            border.width: 1
                        }
                        Text {
                            text: modelData.label
                            font.pixelSize: themeStore.fontCaption
                            color: themeStore.textSecondary
                        }
                    }
                }
                Item { Layout.fillWidth: true }
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.preferredHeight: panel.multiHop ? Math.min(contentHeight, 200) : 0
                visible: panel.multiHop
                clip: true
                interactive: contentHeight > height
                model: panel.multiHop ? panel.hops : []

                delegate: RowLayout {
                    required property var modelData
                    width: list.width
                    height: 30
                    spacing: 10

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: 20
                        height: 20
                        radius: 10
                        color: (modelData.toIndex === panel.currentStep)
                               ? themeStore.statusSuccess : themeStore.borderColor
                        Text {
                            anchors.centerIn: parent
                            text: modelData.toIndex + 1
                            font.pixelSize: themeStore.fontCaption
                            color: themeStore.textColor
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: panel.stopLabel(modelData)
                        font.pixelSize: themeStore.fontBody
                        color: themeStore.textColor
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.alignment: Qt.AlignVCenter
                        text: panel.legMetric(modelData)
                        font.pixelSize: themeStore.fontCaption
                        color: themeStore.textSecondary
                    }
                }
            }
        }
    }
}
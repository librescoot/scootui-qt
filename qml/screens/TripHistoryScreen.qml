import QtQuick
import QtQuick.Layouts
import "../widgets/components"
import "../overlays"

Rectangle {
    id: root
    color: themeStore.isDark ? "black" : "white"
    readonly property color foreground: themeStore.isDark ? "white" : "black"
    readonly property color muted: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
    readonly property bool german: translations.language === "de"
    property int page: 0
    property int selectedIndex: 0
    property string requestedPointsId: ""
    readonly property int rowCount: tripHistoryService.trips.length + (tripHistoryService.hasMore ? 1 : 0)
    readonly property var trip: tripHistoryService.selected
    readonly property bool detailReady: root.page === 1 && !!root.trip.id
    readonly property int validPointCount: {
        var points = tripHistoryService.points, count = 0
        for (var i = 0; i < points.length; ++i) {
            var p = points[i]
            if (!p || p.latitude === null || p.latitude === undefined || p.latitude === ""
                    || p.longitude === null || p.longitude === undefined || p.longitude === "") continue
            var lat = Number(p.latitude), lon = Number(p.longitude)
            if (isFinite(lat) && isFinite(lon) && Math.abs(lat) <= 85 && Math.abs(lon) <= 180) ++count
        }
        return count
    }

    function dateString(seconds) {
        var d = new Date(Number(seconds) * 1000)
        if (Number(seconds) < 1577836800 || isNaN(d.getTime())) return german ? "Datum unbekannt" : "Unknown date"
        return d.toLocaleString(Qt.locale(translations.language), Locale.ShortFormat)
    }
    function metricNumber(value, divisor, decimals) {
        if (value === null || value === undefined || !isFinite(Number(value))) return "--"
        return (Number(value) / divisor).toLocaleString(Qt.locale(translations.language), 'f', decimals)
    }
    function maybeLoadPoints() {
        var id = root.trip.id || ""
        if (root.page !== 1 || tripHistoryService.loading || tripHistoryService.error !== ""
                || id === "" || id === root.requestedPointsId || Number(root.trip.point_count) <= 0) return
        root.requestedPointsId = id
        tripHistoryService.loadPoints()
    }
    function closePage() {
        if (page === 1) {
            page = 0
            requestedPointsId = ""
            tripHistoryService.close()
            return
        }
        tripHistoryService.close()
        screenStore.closeTripHistory()
        menuStore.resume()
    }
    function select() {
        if (page !== 0 || tripHistoryService.loading) return
        if (selectedIndex < tripHistoryService.trips.length) {
            requestedPointsId = ""
            page = 1
            tripHistoryService.selectTrip(tripHistoryService.trips[selectedIndex].id)
        } else if (tripHistoryService.hasMore) {
            tripHistoryService.loadMore()
        }
    }
    Component.onCompleted: tripHistoryService.refresh()
    Component.onDestruction: tripHistoryService.close()
    Connections {
        target: tripHistoryService
        function onChanged() { root.maybeLoadPoints() }
    }
    Connections {
        target: inputHandler
        function onLeftTap() {
            if (root.page === 0 && root.rowCount > 1) root.selectedIndex = (root.selectedIndex + 1) % root.rowCount
        }
        function onRightTap() { root.select() }
        function onLeftHold() { root.closePage() }
        function onLeftBrakeHold() {
            tripHistoryService.close()
            screenStore.closeTripHistory()
            menuStore.close()
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 16
        spacing: 0
        Text {
            objectName: "tripHistoryTitle"
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            text: root.page === 0 ? translations.menuTripHistory.toUpperCase() : (root.german ? "FAHRT" : "TRIP")
            color: root.foreground
            font.pixelSize: themeStore.fontHeading
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
        }
        Item { Layout.preferredHeight: 6 }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ListView {
                objectName: "tripHistoryList"
                anchors.fill: parent
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                clip: true
                visible: root.page === 0
                spacing: 2
                currentIndex: root.selectedIndex
                model: root.rowCount
                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                delegate: MenuItem {
                    width: ListView.view.width
                    isSelected: index === root.selectedIndex
                    hasChildren: index < tripHistoryService.trips.length
                    title: index < tripHistoryService.trips.length
                           ? root.dateString(tripHistoryService.trips[index].started_at_s)
                           : (root.german ? "Weitere Fahrten" : "More trips")
                    valueLabel: index < tripHistoryService.trips.length
                                ? (Number(tripHistoryService.trips[index].distance_m) / 1000).toLocaleString(Qt.locale(translations.language), 'f', 1) + " km" : ""
                }
            }
            ColumnLayout {
                objectName: "tripHistoryDetail"
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.bottomMargin: 8
                visible: root.detailReady
                spacing: 6
                Text {
                    objectName: "tripHistoryDate"
                    Layout.fillWidth: true
                    text: root.dateString(root.trip.started_at_s)
                    color: root.muted
                    font.pixelSize: themeStore.fontCaption
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
                Row {
                    id: metrics
                    objectName: "tripHistoryMetrics"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    property var items: {
                        var result = [
                            {label: root.german ? "Strecke" : "Distance", value: root.metricNumber(root.trip.distance_m, 1000, 1) + " km"},
                            {label: root.german ? "Fahrzeit" : "Ride time", value: root.trip.ready_duration_s === undefined || root.trip.ready_duration_s === null || !isFinite(Number(root.trip.ready_duration_s)) ? "--" : Math.round(Number(root.trip.ready_duration_s) / 60) + " min"},
                            {label: "Ø", value: root.metricNumber(root.trip.average_speed_kmh, 1, 1) + " km/h"}
                        ]
                        if (root.trip.max_recorded_gps_speed_kmh !== null && root.trip.max_recorded_gps_speed_kmh !== undefined)
                            result.push({label: "GPS max", value: root.metricNumber(root.trip.max_recorded_gps_speed_kmh, 1, 1) + " km/h"})
                        return result
                    }
                    Repeater {
                        model: metrics.items
                        Column {
                            required property var modelData
                            width: metrics.width / metrics.items.length
                            spacing: 2
                            Text {
                                width: parent.width
                                text: modelData.label
                                color: root.muted
                                font.pixelSize: themeStore.fontCaption
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: modelData.value
                                color: root.foreground
                                font.pixelSize: themeStore.fontBody
                                minimumPixelSize: 13
                                fontSizeMode: Text.Fit
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                maximumLineCount: 1
                            }
                        }
                    }
                }
                Item {
                    id: mapArea
                    objectName: "tripHistoryMapArea"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    Loader {
                        id: tripMap
                        objectName: "tripHistoryMapLoader"
                        anchors.fill: parent
                        active: root.detailReady && !tripHistoryService.loading && tripHistoryService.error === ""
                                && root.validPointCount > 1 && mapService.styleJson.length > 0
                        source: Qt.resolvedUrl("TripHistoryMap.qml")
                        onStatusChanged: {
                            if (status === Loader.Error && source.toString().indexOf("Legacy") === -1)
                                source = Qt.resolvedUrl("TripHistoryMapLegacy.qml")
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        // Keep the map rendering behind the loading veil.
                        opacity: 0.98
                        color: themeStore.isDark ? "#161616" : "#efefef"
                        visible: !tripMap.active || tripMap.status !== Loader.Ready || !tripMap.item || !tripMap.item.styledOutputReady
                        Text {
                            objectName: "tripHistoryMapStatus"
                            anchors.centerIn: parent
                            width: parent.width - 32
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            color: root.muted
                            font.pixelSize: themeStore.fontBody
                            text: tripHistoryService.loading ? (root.german ? "GPS-Spur wird geladen …" : "Loading GPS trace …")
                                : tripHistoryService.error !== "" ? (root.german ? "GPS-Spur nicht verfügbar" : "GPS trace unavailable")
                                : Number(root.trip.point_count) <= 0 || root.validPointCount < 2 ? (root.german ? "Keine GPS-Spur" : "No GPS trace")
                                : mapService.styleJson.length === 0 || tripMap.status === Loader.Error || (tripMap.item && !tripMap.item.styleValid) ? (root.german ? "Karte nicht verfügbar" : "Map unavailable")
                                : (root.german ? "Karte wird geladen …" : "Loading map …")
                        }
                    }
                }
            }
            Text {
                objectName: "tripHistoryStatus"
                anchors.centerIn: parent
                width: parent.width - 80
                visible: !root.detailReady && (tripHistoryService.loading || tripHistoryService.error !== "" || (root.page === 0 && root.rowCount === 0))
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: root.muted
                font.pixelSize: themeStore.fontBody
                text: tripHistoryService.loading ? (root.german ? "Wird geladen …" : "Loading …")
                    : tripHistoryService.error === "trip not found" ? (root.german ? "Diese Fahrt ist nicht mehr gespeichert" : "This trip is no longer stored")
                    : tripHistoryService.error !== "" ? (root.german ? "Fahrtenverlauf nicht verfügbar" : "Trip history unavailable")
                    : (root.german ? "Noch keine abgeschlossenen Fahrten" : "No completed trips yet")
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: hints.height + 1
            color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.3) : Qt.rgba(1, 1, 1, 0.3)
            Rectangle { width: parent.width; height: 1; color: themeStore.isDark ? "#1AFFFFFF" : "#1F000000" }
            ControlHints {
                id: hints
                objectName: "tripHistoryHints"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                leftTap: root.page === 0 && root.rowCount > 1 ? translations.controlScroll : ""
                leftHold: translations.controlBack
                rightTap: root.page === 0 && root.rowCount > 0 ? translations.controlSelect : ""
            }
        }
    }
}

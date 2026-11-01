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
    readonly property int rowCount: tripHistoryService.trips.length + (tripHistoryService.hasMore ? 1 : 0)
    readonly property var trip: tripHistoryService.selected

    function dateString(seconds) {
        var d = new Date(Number(seconds) * 1000)
        if (Number(seconds) < 1577836800 || isNaN(d.getTime())) return german ? "Datum unbekannt" : "Unknown date"
        return d.toLocaleString(Qt.locale(translations.language), Locale.ShortFormat)
    }
    function closePage() {
        if (page === 2) { tripHistoryService.close(); page = 1; return }
        if (page === 1) { tripHistoryService.close(); page = 0; return }
        tripHistoryService.close()
        screenStore.closeTripHistory()
        menuStore.resume()
    }
    function select() {
        if (tripHistoryService.loading) return
        if (page === 0) {
            if (selectedIndex < tripHistoryService.trips.length) {
                page = 1
                tripHistoryService.selectTrip(tripHistoryService.trips[selectedIndex].id)
            } else if (tripHistoryService.hasMore) {
                tripHistoryService.loadMore()
            }
        } else if (page === 1 && Number(trip.point_count) > 0) {
            page = 2
            tripHistoryService.loadPoints()
        }
    }
    Component.onCompleted: tripHistoryService.refresh()
    Component.onDestruction: tripHistoryService.close()
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
        anchors.topMargin: 24
        spacing: 0
        Text {
            objectName: "tripHistoryTitle"
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            text: root.page === 0 ? translations.menuTripHistory.toUpperCase() : (root.german ? "FAHRT" : "TRIP")
            color: root.foreground
            font.pixelSize: themeStore.fontHeading
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
        }
        Item { Layout.preferredHeight: 8 }
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
            Column {
                anchors.fill: parent
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                spacing: 14
                visible: root.page === 1 && !tripHistoryService.loading && tripHistoryService.error === ""
                Text { width: parent.width; text: root.dateString(root.trip.started_at_s); color: root.foreground; font.pixelSize: themeStore.fontTitle; wrapMode: Text.Wrap }
                Text { width: parent.width; text: (root.german ? "Strecke: " : "Distance: ") + (Number(root.trip.distance_m || 0) / 1000).toLocaleString(Qt.locale(translations.language), 'f', 1) + " km"; color: root.foreground; font.pixelSize: themeStore.fontBody }
                Text { width: parent.width; text: (root.german ? "Fahrzeit: " : "Riding time: ") + Math.round(Number(root.trip.ready_duration_s || 0) / 60) + " min"; color: root.foreground; font.pixelSize: themeStore.fontBody }
                Text { width: parent.width; visible: root.trip.average_speed_kmh !== null && root.trip.average_speed_kmh !== undefined; text: "Ø " + Number(root.trip.average_speed_kmh || 0).toLocaleString(Qt.locale(translations.language), 'f', 1) + " km/h"; color: root.foreground; font.pixelSize: themeStore.fontBody }
                Text { width: parent.width; visible: root.trip.max_recorded_gps_speed_kmh !== null && root.trip.max_recorded_gps_speed_kmh !== undefined; text: "GPS max: " + Number(root.trip.max_recorded_gps_speed_kmh || 0).toLocaleString(Qt.locale(translations.language), 'f', 1) + " km/h"; color: root.foreground; font.pixelSize: themeStore.fontBody }
                MenuItem { width: parent.width; title: Number(root.trip.point_count) > 0 ? (root.german ? "GPS-Spur ansehen" : "View GPS trace") : (root.german ? "Keine GPS-Spur" : "No GPS trace"); isSelected: Number(root.trip.point_count) > 0; hasChildren: isSelected }
            }
            Loader {
                id: tripMap
                objectName: "tripHistoryMapLoader"
                anchors.fill: parent
                active: root.page === 2 && !tripHistoryService.loading && tripHistoryService.error === ""
                        && tripHistoryService.points.length > 1 && mapService.styleJson.length > 0
                source: Qt.resolvedUrl("TripHistoryMap.qml")
                onStatusChanged: {
                    if (status === Loader.Error && source.toString().indexOf("Legacy") === -1)
                        source = Qt.resolvedUrl("TripHistoryMapLegacy.qml")
                }
            }
            Text {
                anchors.centerIn: parent
                width: parent.width - 80
                visible: tripHistoryService.loading || tripHistoryService.error !== "" || (root.page === 0 && root.rowCount === 0)
                         || (root.page === 2 && (tripHistoryService.points.length < 2 || mapService.styleJson.length === 0 || tripMap.status === Loader.Error))
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: root.muted
                font.pixelSize: themeStore.fontBody
                text: tripHistoryService.loading ? (root.german ? "Wird geladen …" : "Loading …")
                    : tripHistoryService.error !== "" ? (root.german ? "Fahrtenverlauf nicht verfügbar" : "Trip history unavailable")
                    : root.page === 2 && (mapService.styleJson.length === 0 || tripMap.status === Loader.Error) ? (root.german ? "Karte nicht verfügbar" : "Map unavailable")
                    : root.page === 2 ? (root.german ? "Keine GPS-Spur" : "No GPS trace")
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
                rightTap: root.page === 0 && root.rowCount > 0 || root.page === 1 && Number(root.trip.point_count) > 0
                          ? translations.controlSelect : ""
            }
        }
    }
}

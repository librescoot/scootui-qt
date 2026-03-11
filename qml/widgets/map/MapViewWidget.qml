import QtQuick
import QtQuick.Layouts

// QMapLibre MapView wrapper
// Uses a Loader to gracefully handle missing QtLocation/QMapLibre plugin
Item {
    id: mapViewWidget
    anchors.fill: parent

    property bool mapReady: typeof mapService !== "undefined" ? mapService.isReady : false

    // Reload map when style URL changes (e.g. theme switch)
    // PluginParameter is only read at creation time, so we must recreate the MapView
    Connections {
        target: typeof mapService !== "undefined" ? mapService : null
        function onStyleUrlChanged() {
            mapLoader.active = false
            mapLoader.active = true
        }
    }

    Loader {
        id: mapLoader
        anchors.fill: parent
        active: true
        source: Qt.resolvedUrl("MapViewContent.qml")
    }

    // Fallback background when map not ready or plugin unavailable
    Rectangle {
        anchors.fill: parent
        visible: !mapReady || mapLoader.status === Loader.Error
        color: typeof themeStore !== "undefined" && themeStore.isDark
               ? "#1a1a2e" : "#e8e8e8"

        Column {
            anchors.centerIn: parent
            spacing: 8
            visible: mapLoader.status === Loader.Error

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Map unavailable"
                color: typeof themeStore !== "undefined" && themeStore.isDark
                       ? "#666" : "#999"
                font.pixelSize: 16
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "QMapLibre plugin not installed"
                color: typeof themeStore !== "undefined" && themeStore.isDark
                       ? "#444" : "#bbb"
                font.pixelSize: 12
            }
        }
    }
}

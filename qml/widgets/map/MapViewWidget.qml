import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

// QMapLibre MapView wrapper
// Uses a Loader to gracefully handle missing QtLocation/QMapLibre plugin
Item {
    id: mapViewWidget
    anchors.fill: parent

    property bool mapReady: MapService.isReady

    // Reload map when style URL changes (e.g. theme switch)
    // PluginParameter is only read at creation time, so we must recreate the MapView
    Connections {
        target: MapService
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

        // MapViewContent uses "import MapLibre.Location" (v4.x).
        // If unavailable, fall back to MapViewContentLegacy which uses "import MapLibre" (v3.x).
        onStatusChanged: {
            if (status === Loader.Error && source.toString().indexOf("Legacy") === -1) {
                source = Qt.resolvedUrl("MapViewContentLegacy.qml")
            }
        }
    }

    // Fallback background when map not ready or plugin unavailable
    Rectangle {
        anchors.fill: parent
        visible: !mapReady || mapLoader.status === Loader.Error
        color: ThemeStore.isDark
               ? "#1a1a2e" : "#e8e8e8"

        Column {
            anchors.centerIn: parent
            spacing: 8
            visible: mapLoader.status === Loader.Error

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Map unavailable"
                color: ThemeStore.isDark
                       ? "#666" : "#999"
                font.pixelSize: ThemeStore.fontBody
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "QMapLibre plugin not installed"
                color: ThemeStore.isDark
                       ? "#444" : "#bbb"
                font.pixelSize: 12
            }
        }
    }
}

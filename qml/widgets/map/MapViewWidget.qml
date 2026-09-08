import QtQuick
import QtQuick.Layouts

// QMapLibre MapView wrapper
// Uses a Loader to gracefully handle missing QtLocation/QMapLibre plugin
Item {
    id: mapViewWidget
    anchors.fill: parent

    property bool mapReady: typeof mapService !== "undefined" ? mapService.isReady : false
    property string styleUrl: typeof mapService !== "undefined" ? mapService.styleUrl : ""
    property bool styleReady: styleUrl.length > 0
    readonly property bool styledOutputReady: mapLoader.status === Loader.Ready
                                               && mapLoader.item !== null
                                               && mapLoader.item.styledOutputReady

    // Reload map when the style URL actually changes (offline/mbtiles or traffic
    // overlay toggle). PluginParameter is only read at creation time, so those
    // need a MapView recreate. Theme switches do NOT change the URL: the map
    // recolors existing layers in place (see MapViewContent.qml), no reload.
    Connections {
        target: typeof mapService !== "undefined" ? mapService : null
        function onStyleUrlChanged() {
            // Never create QMapLibre without a style. Its unstyled native
            // surface clears white and can become ScootUI's first frame.
            mapLoader.active = false
            if (typeof mapService !== "undefined" && mapService.styleUrl.length > 0)
                mapLoader.active = true
        }
    }

    Loader {
        id: mapLoader
        anchors.fill: parent
        active: false
        source: Qt.resolvedUrl("MapViewContent.qml")

        Component.onCompleted: active = mapViewWidget.styleReady

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
        visible: !styleReady || !mapReady || !styledOutputReady
                 || mapLoader.status === Loader.Error
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
                font.pixelSize: themeStore.fontBody
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

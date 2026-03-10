import QtQuick
import QtQuick.Layouts
import QtLocation
import QtPositioning

// QMapLibre MapView wrapper
// Binds camera to mapService properties, handles style loading and offline MBTiles
Item {
    id: mapViewWidget
    anchors.fill: parent

    property bool mapReady: typeof mapService !== "undefined" ? mapService.isReady : false

    MapView {
        id: mapView
        anchors.fill: parent
        visible: mapReady

        map.plugin: Plugin {
            name: "maplibre"
            PluginParameter {
                name: "maplibre.map.styles"
                value: typeof mapService !== "undefined" ? mapService.styleUrl : ""
            }
        }

        map.center: QtPositioning.coordinate(
            typeof mapService !== "undefined" ? mapService.mapLatitude : 0,
            typeof mapService !== "undefined" ? mapService.mapLongitude : 0
        )

        map.zoomLevel: typeof mapService !== "undefined" ? mapService.mapZoom : 16
        map.bearing: typeof mapService !== "undefined" ? mapService.mapBearing : 0

        map.activeMapType: map.supportedMapTypes[0]

        // Disable user interaction (camera is driven by MapService)
        gesture.enabled: false

        // Route polyline
        MapPolyline {
            id: routeBorder
            visible: typeof mapService !== "undefined" && mapService.routeCoordinates.length > 0
            line.width: 6
            line.color: "white"

            path: {
                var p = []
                if (typeof mapService !== "undefined") {
                    var coords = mapService.routeCoordinates
                    for (var i = 0; i < coords.length; i++) {
                        p.push(QtPositioning.coordinate(coords[i].lat, coords[i].lng))
                    }
                }
                return p
            }
        }

        MapPolyline {
            id: routeFill
            visible: routeBorder.visible
            line.width: 4
            line.color: "#42A5F5"  // Blue.shade400

            path: routeBorder.path
        }
    }

    // Fallback background when map not ready
    Rectangle {
        anchors.fill: parent
        visible: !mapReady
        color: typeof themeStore !== "undefined" && themeStore.isDark
               ? "#1a1a2e" : "#e8e8e8"
    }
}

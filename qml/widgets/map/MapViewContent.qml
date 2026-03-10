import QtQuick
import QtLocation
import QtPositioning

MapView {
    id: mapView

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
        line.color: "#42A5F5"

        path: routeBorder.path
    }
}

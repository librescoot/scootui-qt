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
        typeof mapService !== "undefined" && mapService.isReady
            ? mapService.mapLatitude
            : (typeof gpsStore !== "undefined" && gpsStore.latitude !== 0
                ? gpsStore.latitude : 52.520008),
        typeof mapService !== "undefined" && mapService.isReady
            ? mapService.mapLongitude
            : (typeof gpsStore !== "undefined" && gpsStore.longitude !== 0
                ? gpsStore.longitude : 13.404954)
    )

    map.zoomLevel: typeof mapService !== "undefined" ? mapService.mapZoom : 17
    map.bearing: typeof mapService !== "undefined" ? mapService.mapBearing : 0

    // Route polyline - Add to map
    MapPolyline {
        id: routeBorder
        parent: mapView.map
        visible: typeof mapService !== "undefined" && mapService.routeCoordinates.length > 0
        line.width: 10 // Make it even wider for testing
        line.color: "red" // Contrasting color

        function updatePath() {
            var p = []
            if (typeof mapService !== "undefined") {
                var coords = mapService.routeCoordinates
                for (var i = 0; i < coords.length; i++) {
                    p.push(QtPositioning.coordinate(coords[i].latitude, coords[i].longitude))
                }
            }
            path = p
            console.log("MapViewContent: updated route path with " + path.length + " points, visible: " + visible)
        }

        onVisibleChanged: {
            console.log("MapViewContent: routeBorder visible changed to: " + visible + ", coords length: " + (typeof mapService !== "undefined" ? mapService.routeCoordinates.length : "N/A"))
        }

        onPathChanged: {
            console.log("MapViewContent: routeBorder path changed, new length: " + path.length)
        }

        Component.onCompleted: updatePath()

        Connections {
            target: typeof mapService !== "undefined" ? mapService : null
            function onRouteCoordinatesChanged() {
                console.log("MapViewContent: onRouteCoordinatesChanged signal received, coords length: " + mapService.routeCoordinates.length)
                routeBorder.updatePath()
            }
        }
    }

    MapPolyline {
        id: routeFill
        parent: mapView.map
        visible: routeBorder.visible
        line.width: 6
        line.color: "#42A5F5"

        path: routeBorder.path
    }

    // DEBUG: Test marker at first coordinate of route
    MapQuickItem {
        id: debugMarker
        parent: mapView.map
        visible: routeBorder.visible && routeBorder.path.length > 0
        coordinate: routeBorder.path.length > 0 ? routeBorder.path[0] : QtPositioning.coordinate(0,0)
        anchorPoint.x: 10
        anchorPoint.y: 10
        sourceItem: Rectangle {
            width: 20; height: 20
            radius: 10
            color: "yellow"
            border.color: "black"
            border.width: 2
        }
        
        onVisibleChanged: console.log("MapViewContent: Debug marker visible: " + visible)
    }
}

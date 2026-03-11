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

    map.zoomLevel: typeof mapService !== "undefined" ? mapService.mapZoom : 17
    map.bearing: typeof mapService !== "undefined" ? mapService.mapBearing : 0

    function vehicleCoordinate() {
        if (typeof mapService !== "undefined" && mapService.isReady) {
            return QtPositioning.coordinate(mapService.mapLatitude, mapService.mapLongitude)
        }
        if (typeof gpsStore !== "undefined" && gpsStore.latitude !== 0) {
            return QtPositioning.coordinate(gpsStore.latitude, gpsStore.longitude)
        }
        return QtPositioning.coordinate(52.520008, 13.404954)
    }

    function vehicleScreenPoint() {
        var offsetY = typeof mapService !== "undefined" ? mapService.vehicleOffsetY : 0
        return Qt.point(map.width / 2, map.height / 2 + offsetY)
    }

    function updateCamera() {
        if (!map || map.width <= 0 || map.height <= 0) return

        var vehicleCoord = vehicleCoordinate()
        if (!vehicleCoord || !vehicleCoord.isValid) return

        if (typeof mapService !== "undefined" && mapService.isReady) {
            var pt = vehicleScreenPoint()

            // Prefer QtLocation's built-in helper when available.
            if (typeof map.alignCoordinateToPoint === "function") {
                map.alignCoordinateToPoint(vehicleCoord, pt)
                return
            }

            // Fallback: center on vehicle, then offset the center upward in screen pixels.
            map.center = vehicleCoord
            var offsetY = typeof mapService !== "undefined" ? mapService.vehicleOffsetY : 0
            if (offsetY === 0 || typeof map.toCoordinate !== "function") return

            Qt.callLater(function () {
                if (!map || map.width <= 0 || map.height <= 0) return
                var newCenter = map.toCoordinate(Qt.point(map.width / 2, map.height / 2 - offsetY))
                if (newCenter && newCenter.isValid) map.center = newCenter
            })
            return
        }

        map.center = vehicleCoord
    }

    Component.onCompleted: updateCamera()

    onWidthChanged: updateCamera()
    onHeightChanged: updateCamera()

    Connections {
        target: typeof mapService !== "undefined" ? mapService : null
        function onIsReadyChanged() { mapView.updateCamera() }
        function onMapLatitudeChanged() { mapView.updateCamera() }
        function onMapLongitudeChanged() { mapView.updateCamera() }
        function onMapZoomChanged() { mapView.updateCamera() }
        function onMapBearingChanged() { mapView.updateCamera() }
        function onVehicleOffsetYChanged() { mapView.updateCamera() }
    }

    Connections {
        target: typeof gpsStore !== "undefined" ? gpsStore : null
        function onLatitudeChanged() { mapView.updateCamera() }
        function onLongitudeChanged() { mapView.updateCamera() }
    }

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

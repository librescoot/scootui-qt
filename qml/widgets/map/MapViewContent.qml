import QtQuick
import QtLocation
import QtPositioning

MapView {
    id: mapView

    map.plugin: Plugin {
        id: mapPlugin
        name: "maplibre"
        
        // Enable tile caching for offline zoomed-out views
        PluginParameter {
            name: "renderMode"
            value: "gpu"
        }
        
        PluginParameter {
            name: "cache.mode"
            value: "CacheOnlineMode"
        }
        
        PluginParameter {
            name: "cache.diskPath"
            value: "/tmp/qt-map-cache"
        }
        
        PluginParameter {
            name: "maplibre.map.styles"
            value: typeof mapService !== "undefined" ? mapService.styleUrl : ""
        }
        
        PluginParameter {
            name: "maptile.loading.lazy"
            value: true
        }
    }

    map.zoomLevel: typeof mapService !== "undefined" ? mapService.mapZoom : 17
    map.bearing: typeof mapService !== "undefined" ? mapService.mapBearing : 0
    map.tilt: typeof mapService !== "undefined" ? mapService.mapTilt : 85

    function vehicleCoordinate() {
        if (typeof mapService !== "undefined" && mapService.isReady) {
            return QtPositioning.coordinate(mapService.mapLatitude, mapService.mapLongitude)
        }
        if (typeof gpsStore !== "undefined" && gpsStore.latitude !== 0) {
            return QtPositioning.coordinate(gpsStore.latitude, gpsStore.longitude)
        }
        return QtPositioning.coordinate(52.520008, 13.404954)
    }

    function updateCamera() {
        if (!map || map.width <= 0 || map.height <= 0) return

        var vehicleCoord = vehicleCoordinate()
        if (!vehicleCoord || !vehicleCoord.isValid) return

        if (typeof mapService !== "undefined" && mapService.isReady) {
            // mapLatitude/mapLongitude is the vehicle position.
            // Place it at the vehicle screen point (offset below center);
            // Qt handles the bearing-aware pivot so the map rotates around the marker.
            var offsetY = mapService.vehicleOffsetY
            var pt = Qt.point(map.width / 2, map.height / 2 + offsetY)

            if (typeof map.alignCoordinateToPoint === "function") {
                map.alignCoordinateToPoint(vehicleCoord, pt)
                return
            }

            // Fallback: set center then shift by offset in screen space
            map.center = vehicleCoord
            if (offsetY !== 0 && typeof map.toCoordinate === "function") {
                Qt.callLater(function () {
                    if (!map || map.width <= 0 || map.height <= 0) return
                    var shifted = map.toCoordinate(Qt.point(map.width / 2, map.height / 2 - offsetY))
                    if (shifted && shifted.isValid) map.center = shifted
                })
            }
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
        function onMapTiltChanged() { mapView.updateCamera() }
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
        line.width: 6
        line.color: "#ECEFF1"

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
        z: routeBorder.z + 1
        line.width: 4
        line.color: "#42A5F5"

        path: routeBorder.path
    }

}

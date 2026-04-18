import QtQuick
import QtLocation
import QtPositioning
import MapLibre
import ScootUI 1.0

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
            value: MapService.styleUrl
        }
        
        PluginParameter {
            name: "maptile.loading.lazy"
            value: true
        }

        PluginParameter {
            name: "maplibre.items.insert_before"
            value: "building"
        }
    }

    map.zoomLevel: MapService.mapZoom
    map.bearing: MapService.mapBearing
    map.tilt: 85

    function vehicleCoordinate() {
        if (MapService.isReady) {
            return QtPositioning.coordinate(MapService.mapLatitude, MapService.mapLongitude)
        }
        if (GpsStore.latitude !== 0) {
            return QtPositioning.coordinate(GpsStore.latitude, GpsStore.longitude)
        }
        return QtPositioning.coordinate(52.520008, 13.404954)
    }

    function updateCamera() {
        if (!map || map.width <= 0 || map.height <= 0) return

        var vehicleCoord = vehicleCoordinate()
        if (!vehicleCoord || !vehicleCoord.isValid) return

        if (MapService.isReady) {
            // mapLatitude/mapLongitude is the vehicle position.
            // Place it at the vehicle screen point (offset below center);
            // Qt handles the bearing-aware pivot so the map rotates around the marker.
            var offsetY = MapService.vehicleOffsetY
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
        target: MapService
        function onIsReadyChanged() { mapView.updateCamera() }
        function onMapLatitudeChanged() { mapView.updateCamera() }
        function onMapLongitudeChanged() { mapView.updateCamera() }
        function onMapZoomChanged() { mapView.updateCamera() }
        function onMapBearingChanged() { mapView.updateCamera() }
        function onVehicleOffsetYChanged() { mapView.updateCamera() }
    }

    Connections {
        target: GpsStore
        function onLatitudeChanged() { mapView.updateCamera() }
        function onLongitudeChanged() { mapView.updateCamera() }
    }

    // Route rendered as native MapLibre layers (inserted before "building" layer
    // so that 3D building extrusions properly occlude the route line)
    MapLibre.style: Style {
        id: routeStyle

        SourceParameter {
            id: routeSource
            styleId: "route"
            type: "geojson"
            property string data: MapService.routeGeoJson
            onDataChanged: updateNotify()
        }

        LayerParameter {
            styleId: "route-border"
            type: "line"
            property string source: "route"
            layout: { "line-cap": "round", "line-join": "round" }
            paint: { "line-color": "#1565C0", "line-width": 11 }
        }

        LayerParameter {
            styleId: "route-fill"
            type: "line"
            property string source: "route"
            layout: { "line-cap": "round", "line-join": "round" }
            paint: { "line-color": "#42A5F5", "line-width": 7 }
        }
    }

}

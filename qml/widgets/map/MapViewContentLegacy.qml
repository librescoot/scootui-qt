import QtQuick
import QtLocation
import QtPositioning
import MapLibre

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

        PluginParameter {
            name: "maplibre.items.insert_before"
            value: "building"
        }
    }

    map.zoomLevel: typeof mapService !== "undefined" ? mapService.mapZoom : 13
    map.bearing: typeof mapService !== "undefined" ? mapService.mapBearing : 0
    map.tilt: 85

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
        function onVehicleOffsetYChanged() { mapView.updateCamera() }
    }

    Connections {
        target: typeof gpsStore !== "undefined" ? gpsStore : null
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
            property string data: typeof mapService !== "undefined" ? mapService.routeGeoJson : ""
            onDataChanged: updateNotify()
        }

        LayerParameter {
            styleId: "route-border"
            type: "line"
            property string source: "route"
            layout: { "line-cap": "round", "line-join": "round" }
            paint: { "line-color": "#ECEFF1", "line-width": 6 }
        }

        LayerParameter {
            styleId: "route-fill"
            type: "line"
            property string source: "route"
            layout: { "line-cap": "round", "line-join": "round" }
            paint: { "line-color": "#42A5F5", "line-width": 4 }
        }
    }

}

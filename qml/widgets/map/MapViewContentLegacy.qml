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

    map.zoomLevel: typeof mapService !== "undefined" ? mapService.mapZoom : 15
    map.bearing: typeof mapService !== "undefined" ? mapService.mapBearing : 0
    // 3D (default) tilts the map back to show forward perspective; 2D is
    // a flat top-down view.
    map.tilt: (typeof settingsStore !== "undefined" && settingsStore.mapViewMode === 1) ? 0 : 85

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
            paint: { "line-color": "#1565C0", "line-width": 11 }
        }

        LayerParameter {
            styleId: "route-fill"
            type: "line"
            property string source: "route"
            layout: { "line-cap": "round", "line-join": "round" }
            paint: { "line-color": "#42A5F5", "line-width": 7 }
        }

        // Theme recolor: override the paint of existing style layers in place
        // when the dark/light theme flips, so the map does not reload (and
        // flash) the whole style. Overrides come from MapService.mapThemeLayers.
        property var _themeParams: []

        Component {
            id: themeLayerComponent
            LayerParameter {}
        }

        function _applyThemePaint() {
            var dark = (typeof themeStore !== "undefined") && themeStore.isDark
            for (var i = 0; i < _themeParams.length; ++i) {
                var tp = _themeParams[i]
                tp.param.paint = dark ? tp.entry.paintDark : tp.entry.paintLight
            }
        }

        Component.onCompleted: {
            if (typeof mapService === "undefined")
                return
            var dark = (typeof themeStore !== "undefined") && themeStore.isDark
            var model = mapService.mapThemeLayers
            for (var i = 0; i < model.length; ++i) {
                var entry = model[i]
                var param = themeLayerComponent.createObject(routeStyle, {
                    styleId: entry.styleId,
                    type: entry.type,
                    paint: dark ? entry.paintDark : entry.paintLight
                })
                if (param) {
                    routeStyle.addParameter(param)
                    _themeParams.push({ param: param, entry: entry })
                }
            }
            _initBuildingParams()
        }

        // The base style's 3D "buildings" / "building-compat" fill-extrusion
        // layers extrude to render_height. In the flat 2D top-down view we
        // flatten them (height/base = 0) so no 3D building extrusions render;
        // the footprints stay as flat polygons. These LayerParameters override
        // only the height/base paint keys, so they merge with the theme recolor
        // parameters above (MapLibre applies just the stated keys by styleId).
        property var _buildingParams: []

        Component {
            id: buildingParamComponent
            LayerParameter {}
        }

        function _applyBuildings2D() {
            var is2D = (typeof settingsStore !== "undefined") && settingsStore.mapViewMode === 1
            var height = is2D ? 0 : ["get", "render_height"]
            var base = is2D ? 0 : ["get", "render_min_height"]
            for (var i = 0; i < _buildingParams.length; ++i) {
                _buildingParams[i].paint = {
                    "fill-extrusion-height": height,
                    "fill-extrusion-base": base
                }
            }
        }

        function _initBuildingParams() {
            var ids = ["buildings", "building-compat"]
            for (var i = 0; i < ids.length; ++i) {
                var param = buildingParamComponent.createObject(routeStyle, {
                    styleId: ids[i],
                    type: "fill-extrusion"
                })
                if (param) {
                    routeStyle.addParameter(param)
                    _buildingParams.push(param)
                }
            }
            _applyBuildings2D()
        }

        Connections {
            target: typeof settingsStore !== "undefined" ? settingsStore : null
            function onMapViewModeChanged() { routeStyle._applyBuildings2D() }
        }

        Connections {
            target: typeof themeStore !== "undefined" ? themeStore : null
            function onThemeChanged() { routeStyle._applyThemePaint() }
        }
    }

}

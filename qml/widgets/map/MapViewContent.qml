import QtQuick
import QtLocation
import QtPositioning
import MapLibre.Location
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
            value: typeof MapService !== "undefined" ? MapService.styleUrl : ""
        }
        
        PluginParameter {
            name: "maptile.loading.lazy"
            value: true
        }

        PluginParameter {
            name: "maplibre.items.insert_before"
            value: "buildings"
        }
    }

    map.zoomLevel: typeof MapService !== "undefined" ? MapService.mapZoom : 15
    map.bearing: typeof MapService !== "undefined" ? MapService.mapBearing : 0
    // 3D (default) tilts the map back to show forward perspective; 2D is
    // a flat top-down view. In 3D the tilt eases off as the camera zooms in
    // for a maneuver: zoom already tracks distance to the next turn, and at
    // full tilt the street names lie almost flat and cannot be read.
    map.tilt: (typeof SettingsStore !== "undefined" && SettingsStore.mapViewMode === 1)
              ? 0
              : (typeof MapService !== "undefined" ? MapService.mapTilt : 60)

    function vehicleCoordinate() {
        if (typeof MapService !== "undefined" && MapService.isReady) {
            return QtPositioning.coordinate(MapService.mapLatitude, MapService.mapLongitude)
        }
        if (typeof GpsStore !== "undefined" && GpsStore.latitude !== 0) {
            return QtPositioning.coordinate(GpsStore.latitude, GpsStore.longitude)
        }
        return QtPositioning.coordinate(52.520008, 13.404954)
    }

    // Desktop debugging: the vehicle has no touchscreen and the dashboard pins
    // zoom to 15-17.5, so there is otherwise no way to look at the tiles above
    // or below that. Middle click hands the camera back to the dynamic zoom.
    WheelHandler {
        enabled: typeof MapService !== "undefined" && MapService.debugZoomEnabled
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function (event) {
            MapService.debugZoomBy(event.angleDelta.y / 480.0)
        }
    }

    TapHandler {
        enabled: typeof MapService !== "undefined" && MapService.debugZoomEnabled
        acceptedButtons: Qt.MiddleButton
        onTapped: MapService.debugResetZoom()
    }

    function updateCamera() {
        if (!map || map.width <= 0 || map.height <= 0) return

        var vehicleCoord = vehicleCoordinate()
        if (!vehicleCoord || !vehicleCoord.isValid) return

        if (typeof MapService !== "undefined" && MapService.isReady) {
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
        target: typeof MapService !== "undefined" ? MapService : null
        function onIsReadyChanged() { mapView.updateCamera() }
        function onMapLatitudeChanged() { mapView.updateCamera() }
        function onMapLongitudeChanged() { mapView.updateCamera() }
        function onMapZoomChanged() { mapView.updateCamera() }
        function onMapBearingChanged() { mapView.updateCamera() }
        function onVehicleOffsetYChanged() { mapView.updateCamera() }
    }

    Connections {
        target: typeof GpsStore !== "undefined" ? GpsStore : null
        function onLatitudeChanged() { mapView.updateCamera() }
        function onLongitudeChanged() { mapView.updateCamera() }
    }

    // Route rendered as native MapLibre layers, inserted before the buildings
    // layer so the 3D extrusions occlude it. The anchor has to be "buildings":
    // "building" is the singular compat layer kept for pre-rename tilesets, and
    // it sits last in the style, which put the route on top of everything.
    MapLibre.style: Style {
        id: routeStyle

        SourceParameter {
            id: routeSource
            styleId: "route"
            type: "geojson"
            property string data: typeof MapService !== "undefined" ? MapService.routeGeoJson : ""
            onDataChanged: updateNotify()
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
            var dark = (typeof ThemeStore !== "undefined") && ThemeStore.isDark
            for (var i = 0; i < _themeParams.length; ++i) {
                var tp = _themeParams[i]
                tp.param.paint = dark ? tp.entry.paintDark : tp.entry.paintLight
            }
        }

        Component.onCompleted: {
            if (typeof MapService === "undefined")
                return
            var dark = (typeof ThemeStore !== "undefined") && ThemeStore.isDark
            var model = MapService.mapThemeLayers
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
        }

        // Buildings are flattened for 2D in the style MapService emits, not
        // here. Collapsing a fill-extrusion to zero height leaves it on the
        // extrusion path and measured slower than the 3D view it replaced.

        Connections {
            target: typeof ThemeStore !== "undefined" ? ThemeStore : null
            function onThemeChanged() { routeStyle._applyThemePaint() }
        }
    }

}

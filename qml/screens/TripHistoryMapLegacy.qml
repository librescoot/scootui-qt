import QtQuick
import QtLocation
import QtPositioning
import MapLibre

MapView {
    id: root
    property bool styledOutputReady: false
    // Attach to the inner Map because the plugin identifies MapView by class name.
    map.MapLibre.style: Style {
        id: historyStyle
        onStyleLoadingStarted: root.styledOutputReady = false
        onFirstStyledFrameRendered: root.styledOutputReady = true
        property var themeParams: []
        Component { id: themeLayer; LayerParameter {} }
        function applyTheme() {
            for (var i = 0; i < themeParams.length; ++i) {
                var item = themeParams[i]
                item.param.paint = themeStore.isDark ? item.entry.paintDark : item.entry.paintLight
            }
        }
        Component.onCompleted: {
            var layers = mapService.mapThemeLayers
            for (var i = 0; i < layers.length; ++i) {
                var entry = layers[i]
                var param = themeLayer.createObject(historyStyle, {
                    styleId: entry.styleId,
                    type: entry.type,
                    paint: themeStore.isDark ? entry.paintDark : entry.paintLight
                })
                if (param) {
                    historyStyle.addParameter(param)
                    themeParams.push({param: param, entry: entry})
                }
            }
        }
        Connections {
            target: themeStore
            function onThemeChanged() { historyStyle.applyTheme() }
        }
    }
    readonly property var points: tripHistoryService.points
    readonly property var bounds: calculateBounds(points)
    readonly property string traceJson: makeTrace(points)
    readonly property string endpointsJson: makeEndpoints(points)
    readonly property bool styleValid: historyStyleJson !== "{}"
    readonly property string historyStyleJson: {
        try {
            var style = JSON.parse(mapService.styleJson)
            style.sources.route.data = JSON.parse(traceJson)
            var hiddenSources = ["plan", "plan-stops", "overview-traveled", "overview-markers", "ride-group-peers"]
            for (var i = 0; i < hiddenSources.length; ++i) {
                var source = style.sources[hiddenSources[i]]
                if (source) source.data = {type: "FeatureCollection", features: []}
            }
            // Where the ride started and ended. Appended last so the markers stay
            // on top of the trace; the colours match the route-plan trip ends.
            style.sources["trip-endpoints"] = {type: "geojson", data: JSON.parse(endpointsJson)}
            style.layers = style.layers.concat([
                {id: "trip-start", type: "circle", source: "trip-endpoints",
                 filter: ["==", "kind", "start"],
                 paint: {"circle-color": "#2E7D32", "circle-radius": 9,
                         "circle-stroke-color": "#ffffff", "circle-stroke-width": 2}},
                {id: "trip-end", type: "circle", source: "trip-endpoints",
                 filter: ["==", "kind", "end"],
                 paint: {"circle-color": "#C62828", "circle-radius": ["case", ["get", "coincident"], 5, 9],
                         "circle-stroke-color": "#ffffff", "circle-stroke-width": 2}}
            ])
            return JSON.stringify(style)
        } catch (e) { return "{}" }
    }
    function calculateBounds(items) {
        var minLat = 90, maxLat = -90, minLon = 180, maxLon = -180, count = 0
        for (var i = 0; i < items.length; ++i) {
            if (!validCoordinate(items[i])) continue
            var lat = Number(items[i].latitude), lon = Number(items[i].longitude)
            minLat = Math.min(minLat, lat); maxLat = Math.max(maxLat, lat)
            minLon = Math.min(minLon, lon); maxLon = Math.max(maxLon, lon); ++count
        }
        if (!count) return { lat: 0, lon: 0, xSpan: 0, ySpan: 0, count: 0 }
        function mercatorY(lat) {
            var radians = lat * Math.PI / 180
            return (1 - Math.log(Math.tan(Math.PI / 4 + radians / 2)) / Math.PI) / 2
        }
        var top = mercatorY(maxLat), bottom = mercatorY(minLat)
        var centerLat = Math.atan(Math.sinh(Math.PI * (1 - top - bottom))) * 180 / Math.PI
        return { lat: centerLat, lon: (minLon + maxLon) / 2,
                 xSpan: (maxLon - minLon) / 360, ySpan: bottom - top, count: count }
    }
    function calculateZoom(box, width, height) {
        if (!box.count) return 2
        var usableWidth = Math.max(1, width - 60), usableHeight = Math.max(1, height - 60)
        var zoomX = box.xSpan > 0 ? Math.log(usableWidth / (256 * box.xSpan)) / Math.LN2 : 17
        var zoomY = box.ySpan > 0 ? Math.log(usableHeight / (256 * box.ySpan)) / Math.LN2 : 17
        return Math.max(2, Math.min(17, zoomX, zoomY))
    }
    function validCoordinate(point) {
        if (!point || point.latitude === null || point.latitude === undefined || point.latitude === ""
                || point.longitude === null || point.longitude === undefined || point.longitude === "") return false
        var lat = Number(point.latitude), lon = Number(point.longitude)
        return isFinite(lat) && isFinite(lon) && Math.abs(lat) <= 85 && Math.abs(lon) <= 180
    }
    // Coincident endpoints use a smaller end marker so both colours remain visible.
    function makeEndpoints(items) {
        var first = null, last = null
        for (var i = 0; i < items.length; ++i) {
            if (!validCoordinate(items[i])) continue
            var point = [Number(items[i].longitude), Number(items[i].latitude)]
            if (first === null) first = point
            last = point
        }
        var features = []
        if (first !== null) {
            features.push({type: "Feature", geometry: {type: "Point", coordinates: first},
                           properties: {kind: "start"}})
            features.push({type: "Feature", geometry: {type: "Point", coordinates: last},
                           properties: {kind: "end", coincident: last[0] === first[0] && last[1] === first[1]}})
        }
        return JSON.stringify({type: "FeatureCollection", features: features})
    }
    function makeTrace(items) {
        var lines = [], line = [], previous = 0
        for (var i = 0; i < items.length; ++i) {
            if (!validCoordinate(items[i])) {
                if (line.length > 1) lines.push(line)
                line = []; previous = 0
                continue
            }
            var lat = Number(items[i].latitude), lon = Number(items[i].longitude), time = Number(items[i].timestamp_ms)
            if (previous && time && (time - previous > 180000 || time < previous)) {
                if (line.length > 1) lines.push(line)
                line = []
            }
            line.push([lon, lat]); previous = time
        }
        if (line.length > 1) lines.push(line)
        return JSON.stringify({type:"Feature", geometry:{type:lines.length === 1 ? "LineString" : "MultiLineString", coordinates:lines.length === 1 ? lines[0] : lines}})
    }
    map.plugin: Plugin {
        name: "maplibre"
        PluginParameter { name: "renderMode"; value: "gpu" }
        PluginParameter { name: "cache.mode"; value: "CacheOnlineMode" }
        PluginParameter { name: "cache.diskPath"; value: "/tmp/qt-map-cache" }
        PluginParameter { name: "maplibre.map.style_json"; value: root.historyStyleJson }
        PluginParameter { name: "maplibre.items.insert_before"; value: "buildings" }
    }
    map.center: QtPositioning.coordinate(bounds.lat, bounds.lon)
    map.zoomLevel: calculateZoom(bounds, map.width, map.height)
    map.bearing: 0
    map.tilt: 0
}

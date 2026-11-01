import QtQuick
import QtLocation
import QtPositioning
import MapLibre.Location

MapView {
    id: root
    readonly property var points: tripHistoryService.points
    readonly property var bounds: calculateBounds(points)
    readonly property string traceJson: makeTrace(points)
    readonly property string historyStyleJson: {
        try {
            var style = JSON.parse(mapService.styleJson)
            style.sources.route.data = JSON.parse(traceJson)
            return JSON.stringify(style)
        } catch (e) { return mapService.styleJson }
    }
    function calculateBounds(items) {
        var minLat = 90, maxLat = -90, minLon = 180, maxLon = -180, count = 0
        for (var i = 0; i < items.length; ++i) {
            var lat = Number(items[i].latitude), lon = Number(items[i].longitude)
            if (!isFinite(lat) || !isFinite(lon) || Math.abs(lat) > 85 || Math.abs(lon) > 180) continue
            minLat = Math.min(minLat, lat); maxLat = Math.max(maxLat, lat)
            minLon = Math.min(minLon, lon); maxLon = Math.max(maxLon, lon); ++count
        }
        return { lat: (minLat + maxLat) / 2, lon: (minLon + maxLon) / 2,
                 span: Math.max(maxLat - minLat, maxLon - minLon, 0.001), count: count }
    }
    function makeTrace(items) {
        var lines = [], line = [], previous = 0
        for (var i = 0; i < items.length; ++i) {
            var lat = Number(items[i].latitude), lon = Number(items[i].longitude), time = Number(items[i].timestamp_ms)
            if (!isFinite(lat) || !isFinite(lon) || Math.abs(lat) > 85 || Math.abs(lon) > 180) continue
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
    map.zoomLevel: Math.max(2, Math.min(17, Math.log(250 / bounds.span) / Math.LN2 - 1))
    map.bearing: 0
    map.tilt: 0
}

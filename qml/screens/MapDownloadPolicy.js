.pragma library

// Download policy for the Navigation Setup screen, kept free of QML so it can
// be tested directly. The service resolves a region from coordinates only when
// the slug is unknown, so a known region needs no GPS fix.

function regionResolved(regionName) {
    return regionName !== ""
}

function gpsRequired(regionName, willDownloadAnything) {
    return willDownloadAnything && !regionResolved(regionName)
}

function canDownload(status, statusIdle, isOnline, hasGps, regionName, willDownloadAnything) {
    if (status !== statusIdle || !isOnline || !willDownloadAnything)
        return false
    return hasGps || regionResolved(regionName)
}

// A component is fetched when it is missing or has its own update.
function willDownload(shown, componentOk, updateAvailable) {
    return shown && (!componentOk || updateAvailable)
}

function sizeLabel(regionName, displayBytes, routingBytes, wantDisplay, wantRouting) {
    var total = 0
    if (wantDisplay) total += Math.max(0, displayBytes)
    if (wantRouting) total += Math.max(0, routingBytes)
    if (total <= 0) return regionName
    return regionName + " (" + Math.round(total / 1048576) + " MB)"
}

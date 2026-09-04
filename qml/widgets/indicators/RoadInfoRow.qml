import QtQuick

// Speed-limit sign followed by the road-name pill. The cluster and the map
// both carry this pairing, so it lives here rather than twice: the two screens
// differ only in where they anchor it and how large the name reads.
//
// Either half can hide on its own — the limit sign when there is no numeric
// limit, the pill when there is no name, and both under their visibility
// settings — and the Row closes the gap on whichever is left.
Row {
    id: root

    // Type size for the road name. The map has room for a larger label than
    // the gap between the speedometer's arc endpoints does.
    property real fontSize: 14
    property real maxTextWidth: 200

    // True on the map screen, so the "Map Only" visibility setting can tell
    // the two hosts apart.
    property bool onMapScreen: false

    spacing: 4

    SpeedLimitIndicator {
        iconSize: 36
        onMapScreen: root.onMapScreen
        anchors.verticalCenter: parent.verticalCenter
    }

    RoadNameDisplay {
        fontSize: root.fontSize
        maxTextWidth: root.maxTextWidth
        onMapScreen: root.onMapScreen
        anchors.verticalCenter: parent.verticalCenter
    }
}

import QtQuick
import "../../theme"

Rectangle {
    id: root
    property string roadName: typeof speedLimitStore !== "undefined" ? speedLimitStore.roadName : ""
    property string roadType: typeof speedLimitStore !== "undefined" ? speedLimitStore.roadType : ""
    property real fontSize: Theme.fontCaption

    visible: roadName.length > 0
    width: label.width + 8
    height: label.height + 4
    radius: Theme.radiusBar

    // German road sign styling based on road type
    color: {
        switch (roadType.toLowerCase()) {
            case "motorway":
            case "trunk":       return "#1565C0"  // blue
            case "primary":     return "#FFB300"  // amber
            case "secondary":   return "#FFFFFF"
            case "tertiary":    return "#FFFFFF"
            case "residential":
            case "living_street": return "#EEEEEE"
            default:            return "#F5F5F5"
        }
    }
    border.width: {
        switch (roadType.toLowerCase()) {
            case "secondary":   return 1
            case "tertiary":    return 0.5
            case "motorway":
            case "trunk":
            case "primary":
            case "residential":
            case "living_street": return 0
            default:            return 0.5
        }
    }
    border.color: {
        switch (roadType.toLowerCase()) {
            case "secondary":   return "#8A000000"
            case "tertiary":    return "#61000000"
            default:            return "#9E9E9E"
        }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.roadName
        font.pixelSize: root.fontSize
        font.weight: Font.Medium
        elide: Text.ElideRight
        maximumLineCount: 1
        width: Math.min(implicitWidth, 140)
        color: {
            switch (root.roadType.toLowerCase()) {
                case "motorway":
                case "trunk":       return "#FFFFFF"
                default:            return "#DD000000"
            }
        }
    }
}

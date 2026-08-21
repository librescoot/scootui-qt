import QtQuick
import ScootUI 1.0

Rectangle {
    id: root
    property string roadName: typeof SpeedLimitStore !== "undefined" ? SpeedLimitStore.roadName : ""
    property string roadRefs: typeof SpeedLimitStore !== "undefined" ? SpeedLimitStore.roadRefs : ""
    property string roadType: typeof SpeedLimitStore !== "undefined" ? SpeedLimitStore.roadType : ""
    property real fontSize: ThemeStore.fontCaption
    property real maxTextWidth: 200

    visible: roadName.length > 0
    width: label.width + 8
    height: label.height + 4
    radius: ThemeStore.radiusBar

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
        text: root.roadRefs.length > 0
              ? root.roadName + " (" + root.roadRefs + ")"
              : root.roadName
        font.pixelSize: root.fontSize
        font.weight: Font.Medium
        elide: Text.ElideRight
        maximumLineCount: 1
        width: Math.min(implicitWidth, root.maxTextWidth)
        color: {
            switch (root.roadType.toLowerCase()) {
                case "motorway":
                case "trunk":       return "#FFFFFF"
                default:            return "#DD000000"
            }
        }
    }
}

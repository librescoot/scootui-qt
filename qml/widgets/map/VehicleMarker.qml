import QtQuick

Item {
    id: vehicleMarker
    width: 37
    height: 37

    property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    property bool isNavigating: typeof navigationService !== "undefined" ? navigationService.isNavigating : false

    // Circle with border
    Rectangle {
        anchors.centerIn: parent
        width: 37
        height: 37
        radius: 18.5
        color: isDark ? "#40C8F0" : "#0090B8"
        border.width: 3
        border.color: "white"

        // Navigation arrow (triangle pointing up)
        Canvas {
            anchors.centerIn: parent
            width: 16
            height: 16
            visible: vehicleMarker.isNavigating

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = "white"
                ctx.beginPath()
                ctx.moveTo(8, 2)
                ctx.lineTo(14, 14)
                ctx.lineTo(8, 10)
                ctx.lineTo(2, 14)
                ctx.closePath()
                ctx.fill()
            }
        }

        // Simple dot when not navigating
        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 4
            color: "white"
            visible: !vehicleMarker.isNavigating
        }
    }
}

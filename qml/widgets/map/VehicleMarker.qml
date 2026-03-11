import QtQuick

Item {
    id: vehicleMarker
    width: 37
    height: 37

    property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    property bool isNavigating: typeof navigationService !== "undefined" ? navigationService.isNavigating : false

    // Circle with border - matching Flutter's grey background style
    Rectangle {
        anchors.centerIn: parent
        width: 37
        height: 37
        radius: 18.5
        color: isDark ? Qt.rgba(0.26, 0.26, 0.26, 0.7) : Qt.rgba(0.88, 0.88, 0.88, 0.7)
        border.width: 1
        border.color: isDark ? Qt.rgba(0.46, 0.46, 0.46, 0.9) : Qt.rgba(0.62, 0.62, 0.62, 0.9)

        // Navigation arrow (blue, matching Flutter Icons.navigation)
        Canvas {
            anchors.centerIn: parent
            width: 24
            height: 24

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = "#2196F3"
                ctx.beginPath()
                // Navigation arrow icon shape
                ctx.moveTo(12, 2)
                ctx.lineTo(20, 20)
                ctx.lineTo(12, 15)
                ctx.lineTo(4, 20)
                ctx.closePath()
                ctx.fill()
            }
        }
    }
}

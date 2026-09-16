import QtQuick
import QtQuick.Layouts
import ScootUI 1.0
import "../widgets/components"

Item {
    id: shortcutOverlay
    anchors.fill: parent
    visible: shortcutMenuStore.visible

    property Item blurSource
    property bool isDark: themeStore.isDark
    readonly property var selectedAction: shortcutMenuStore.actionCount > 0
                                          ? shortcutMenuStore.actions[shortcutMenuStore.selectedIndex]
                                          : ({})
    readonly property string selectedLabel: {
        if (!selectedAction || selectedAction.kind === undefined)
            return ""
        if (selectedAction.kind === "destination")
            return selectedAction.label
        if (selectedAction.kind === "route-overview")
            return translations.shortcutRouteOverview
        if (selectedAction.kind === "stop-navigation")
            return translations.menuStopNavigation
        return screenStore.currentScreen === Scooter.ScreenMode.Cluster
             ? translations.shortcutViewMap : translations.shortcutViewCluster
    }

    function actionIcon(action) {
        if (action.kind === "view")
            return screenStore.currentScreen === Scooter.ScreenMode.Cluster
                 ? MaterialIcon.iconMap : MaterialIcon.iconSpeed
        if (action.kind === "route-overview")
            return MaterialIcon.iconMap
        if (action.kind === "stop-navigation")
            return MaterialIcon.iconCancel
        switch (action.quickIcon) {
        case "home": return MaterialIcon.iconHome
        case "work": return MaterialIcon.iconWork
        case "favorite": return MaterialIcon.iconStar
        default: return MaterialIcon.iconPlace
        }
    }

    Item {
        id: containerWrapper
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: confirmBar.anchors.bottomMargin + confirmBar.height + 6
        width: shortcutMenuStore.actionCount === 1 ? 280
             : shortcutMenuStore.actionCount === 2 ? 340 : 400
        height: 150
        clip: true

        FrostedGlass {
            anchors.fill: parent
            sourceItem: shortcutOverlay.blurSource
            sourceOffset: Qt.point(containerWrapper.x, containerWrapper.y)
            blurAmount: 0.5
            radius: themeStore.radiusModal
            tintColor: isDark ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(1, 1, 1, 0.55)
        }

        Rectangle {
            anchors.fill: parent
            radius: themeStore.radiusModal
            color: "transparent"
            border.width: 2
            border.color: isDark ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.3)
        }

        Row {
            id: contentRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            height: 80

            Repeater {
                model: shortcutMenuStore.actions

                Item {
                    required property var modelData
                    required property int index
                    width: contentRow.width / shortcutMenuStore.actionCount
                    height: contentRow.height

                    Rectangle {
                        id: menuItemRect
                        anchors.centerIn: parent
                        property bool isSelected: index === shortcutMenuStore.selectedIndex
                        property color itemColor: isSelected ? "#FF9800" : (isDark ? "#FFFFFF" : "#212121")
                        width: isSelected ? 80 : 60
                        height: isSelected ? 80 : 60
                        radius: themeStore.radiusModal
                        color: isSelected ? Qt.rgba(1, 0.6, 0, 0.15) : "transparent"
                        border.width: isSelected ? 4 : 2
                        border.color: itemColor

                        Behavior on width { NumberAnimation { duration: 200 } }
                        Behavior on height { NumberAnimation { duration: 200 } }

                        Text {
                            anchors.centerIn: parent
                            font.family: "Material Icons"
                            font.pixelSize: menuItemRect.isSelected ? 36 : 28
                            color: menuItemRect.itemColor
                            text: shortcutOverlay.actionIcon(modelData)
                            Behavior on font.pixelSize { NumberAnimation { duration: 200 } }
                        }
                    }
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: contentRow.bottom
            anchors.topMargin: 6
            width: parent.width - 24
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            maximumLineCount: 1
            text: shortcutOverlay.selectedLabel
            font.pixelSize: themeStore.fontBody
            font.weight: Font.Medium
            color: isDark ? "#FFFFFF" : "#212121"
        }
    }

    Rectangle {
        id: confirmBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        width: Math.max(280, containerWrapper.width)
        height: confirmContent.height + 22
        radius: themeStore.radiusModal
        color: isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.95)
        border.width: 2
        border.color: "#FF9800"
        visible: shortcutMenuStore.confirming

        Column {
            id: confirmContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Row {
                width: parent.width
                spacing: 10

                TintedImage {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    height: 22
                    source: "qrc:/ScootUI/assets/icons/librescoot-seatbox-button.svg"
                    tintColor: "#FF9800"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 32
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    text: shortcutOverlay.selectedAction.kind === "destination"
                          ? translations.shortcutStartDestination.arg(shortcutOverlay.selectedLabel)
                          : shortcutOverlay.selectedLabel
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Bold
                    color: isDark ? "#FFFFFF" : "#000000"
                }
            }

            Rectangle {
                width: parent.width
                height: 6
                radius: 3
                color: isDark ? "#3DFFFFFF" : "#1F000000"

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * (1.0 - confirmProgress)
                    radius: 3
                    color: "#FF9800"
                    property real confirmProgress

                    NumberAnimation on confirmProgress {
                        id: confirmAnim
                        from: 0
                        to: 1
                        duration: shortcutMenuStore.confirmTimeoutMs
                    }
                }
            }
        }

        onVisibleChanged: {
            if (visible) {
                confirmAnim.stop()
                confirmAnim.start()
            }
        }
    }
}

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
        if (selectedAction.kind === "theme") {
            if (themeStore.isAutoMode)
                return translations.shortcutThemeDark
            if (themeStore.isDark)
                return translations.shortcutThemeLight
            return translations.shortcutThemeAuto
        }
        if (selectedAction.kind === "keep-stop")
            return translations.shortcutKeepStop
        if (selectedAction.kind === "route-overview")
            return translations.shortcutRouteOverview
        if (selectedAction.kind === "skip-stop")
            return translations.shortcutSkipStop
        if (selectedAction.kind === "stop-navigation")
            return translations.menuStopNavigation
        if (selectedAction.kind === "debug-overlay")
            return translations.shortcutDebugOverlay
        if (selectedAction.kind === "motion-debug")
            return translations.shortcutMotionDebug
        return screenStore.currentScreen === Scooter.ScreenMode.Cluster
             ? translations.shortcutViewMap : translations.shortcutViewCluster
    }

    // Equal-width cells; every tile scales to its cell, leaving a fixed gap.
    readonly property real cellWidth: shortcutMenuStore.actionCount > 0
                                      ? (contentRow.width
                                         - Math.max(0, shortcutMenuStore.actionCount - 1)
                                           * contentRow.spacing) / shortcutMenuStore.actionCount
                                      : contentRow.width

    function actionIcon(action) {
        if (action.kind === "theme") {
            if (themeStore.isAutoMode)
                return MaterialIcon.iconDarkMode
            if (themeStore.isDark)
                return MaterialIcon.iconLightMode
            return MaterialIcon.iconContrast
        }
        if (action.kind === "view")
            return screenStore.currentScreen === Scooter.ScreenMode.Cluster
                 ? MaterialIcon.iconMap : MaterialIcon.iconSpeed
        if (action.kind === "keep-stop")
            return MaterialIcon.iconPlace
        if (action.kind === "route-overview")
            return MaterialIcon.iconMap
        if (action.kind === "skip-stop")
            return MaterialIcon.iconArrowForward
        if (action.kind === "stop-navigation")
            return MaterialIcon.iconCancel
        if (action.kind === "debug-overlay")
            return MaterialIcon.iconBugReport
        if (action.kind === "motion-debug")
            return MaterialIcon.iconNavigation
        switch (action.icon) {
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
        // Same width for every action count, so the row and confirm bar hold still.
        width: 400
        height: 110
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
            spacing: 4

            Repeater {
                model: shortcutMenuStore.actions

                Item {
                    required property var modelData
                    required property int index
                    width: shortcutOverlay.cellWidth
                    height: contentRow.height

                    Rectangle {
                        id: menuItemRect
                        anchors.centerIn: parent
                        property bool isSelected: index === shortcutMenuStore.selectedIndex
                        property color itemColor: isSelected ? "#FF9800" : (isDark ? "#FFFFFF" : "#212121")
                        readonly property real selectedTileSize: Math.min(80, shortcutOverlay.cellWidth)
                        readonly property real unselectedTileSize: Math.min(60, selectedTileSize * 0.75)
                        width: isSelected ? selectedTileSize : unselectedTileSize
                        height: isSelected ? selectedTileSize : unselectedTileSize
                        radius: themeStore.radiusModal
                        color: isSelected ? Qt.rgba(1, 0.6, 0, 0.15) : "transparent"
                        border.width: isSelected ? 4 : 2
                        border.color: itemColor

                        Behavior on width { NumberAnimation { duration: 200 } }
                        Behavior on height { NumberAnimation { duration: 200 } }

                        Text {
                            anchors.centerIn: parent
                            font.family: "Material Icons"
                            font.pixelSize: Math.max(16, menuItemRect.width
                                                     * (menuItemRect.isSelected ? 0.45 : 0.47))
                            color: menuItemRect.itemColor
                            text: shortcutOverlay.actionIcon(modelData)
                            Behavior on font.pixelSize { NumberAnimation { duration: 200 } }
                        }
                    }
                }
            }
        }
    }

    // Same slot as the confirm bar: the full item name and how to activate it
    // sit exactly where the countdown appears, so the two swap in place.
    Rectangle {
        id: hintBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: confirmBar.anchors.bottomMargin
        width: confirmBar.width
        height: confirmBar.height
        radius: themeStore.radiusModal
        color: isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.95)
        border.width: 2
        border.color: "#FF9800"
        visible: !shortcutMenuStore.confirming
                 && shortcutMenuStore.actionCount > 0

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 4

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                maximumLineCount: 1
                text: shortcutOverlay.selectedLabel
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
                color: isDark ? "#FFFFFF" : "#000000"
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6

                TintedImage {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: "qrc:/ScootUI/assets/icons/librescoot-seatbox-button.svg"
                    tintColor: "#FF9800"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: translations.shortcutReleaseHint
                    font.pixelSize: themeStore.fontCaption
                    color: isDark ? "#FFFFFF" : "#000000"
                    opacity: 0.8
                }
            }
        }
    }

    Rectangle {
        id: confirmBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        width: containerWrapper.width
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

            CountdownBar {
                width: parent.width
                height: 6
                transitionMs: shortcutMenuStore.confirmTimeoutMs
                active: shortcutMenuStore.confirming
                remainingFraction: active ? 0 : 1
            }
        }
    }
}

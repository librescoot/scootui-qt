import QtQuick
import "../widgets/navigation"

Item {
    id: dock
    property var service: typeof notificationService !== "undefined" ? notificationService : null
    property bool isDark: typeof themeStore !== "undefined" && themeStore ? themeStore.isDark : true
    readonly property var presentation: service ? service.presentation : ({})
    readonly property var main: presentation.main || ({})
    readonly property var companion: presentation.companion || ({})
    readonly property bool hasMain: Object.keys(main).length > 0
    readonly property bool hasCompanion: Object.keys(companion).length > 0

    readonly property real maximumHeight: 156

    readonly property bool overviewActive: typeof mapService !== "undefined" && mapService !== null
                                            && mapService.routeOverviewActive

    width: parent ? parent.width : 480
    height: hasMain ? mainRenderer.height + (hasCompanion ? companionCard.height : 0) : 0
    visible: hasMain

    Loader {
        id: mainRenderer
        objectName: "attentionMainRenderer"
        width: parent.width
        height: item ? item.implicitHeight : 0
        active: dock.hasMain
        sourceComponent: dock.main.kind === "nav" ? navigationRenderer : notificationRenderer
    }
    Component {
        id: notificationRenderer
        NotificationCard {
            objectName: "attentionMainCard"
            maximumHeight: dock.maximumHeight - (dock.hasCompanion ? companionCard.height : 0)
            compact: dock.hasCompanion
            entry: dock.main
            queuedCounts: dock.presentation.queuedCounts || ({})
            isDark: dock.isDark
        }
    }
    Component {
        id: navigationRenderer
        TurnByTurnWidget {
            objectName: "attentionTurnByTurn"
            paired: dock.hasCompanion
            overviewOnly: dock.overviewActive
            // The companion card yields height to the main card, so only the
            // unpaired card needs the dock budget to stay clear of the speed
            // readout below. A budget here would also loop with the
            // companion's own budget, which depends on this card's height.
            maximumHeight: dock.hasCompanion ? Infinity : dock.maximumHeight
            queuedCounts: dock.presentation.queuedCounts || ({})
            maneuver: dock.main
            isDark: dock.isDark
        }
    }

    Loader {
        id: companionCard
        anchors.top: mainRenderer.bottom
        width: parent.width
        height: item ? item.implicitHeight : 0
        active: dock.hasCompanion
        sourceComponent: dock.companion.kind === "nav" ? compactNavigationRenderer : companionNotificationRenderer
    }

    Component {
        id: compactNavigationRenderer
        TurnByTurnWidget {
            objectName: "attentionCompanion"
            compact: true
            overviewOnly: dock.overviewActive
            maneuver: dock.companion
            isDark: dock.isDark
        }
    }

    Component {
        id: companionNotificationRenderer
        NotificationCard {
            objectName: "attentionCompanionNotification"
            compact: true
            secondary: true
            maximumHeight: dock.maximumHeight - mainRenderer.height
            entry: dock.companion
            isDark: dock.isDark
        }
    }
}

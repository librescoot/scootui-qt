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
            entry: dock.main
            criticalCount: dock.presentation.criticalCount || 0
            isDark: dock.isDark
        }
    }
    Component {
        id: navigationRenderer
        TurnByTurnWidget {
            objectName: "attentionTurnByTurn"
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
        sourceComponent: TurnByTurnWidget {
            objectName: "attentionCompanion"
            compact: true
            maneuver: dock.companion
            isDark: dock.isDark
        }
    }
}

import QtQuick
import QtTest
import "../qml/notifications"

TestCase {
    name: "UnifiedAttentionDock"
    when: windowShown
    visible: true
    width: 480
    height: 128

    QtObject {
        id: service
        property var presentation: ({})
        property int occupiedHeight: 0
    }

    UnifiedAttentionDock {
        id: dock
        width: 480
        service: service
    }

    function init() {
        dock.isDark = true
        service.presentation = ({main: {}, companion: {}, criticalCount: 0, height: 0})
        service.occupiedHeight = 0
    }

    function show(main, companion, height, criticalCount) {
        service.presentation = ({main: main, companion: companion,
                                 criticalCount: criticalCount || 0, height: height})
        service.occupiedHeight = height
        waitForRendering(dock)
    }

    function test_boundedWarningCompanion() {
        show({id: "warning", kind: "warning", priority: 2, title: "Warning", body: "Test"},
             {id: "navigation-session", kind: "nav", priority: 3, distance: 120,
              maneuverType: 5, street: "Main Street"}, 110)
        compare(dock.height, 110)
        verify(dock.hasCompanion)
        verify(dock.height <= 128)
        compare(findChild(dock, "notificationTitle").font.pixelSize, 24)
        compare(findChild(dock, "notificationBody").font.pixelSize, 20)
        compare(findChild(dock, "notificationCompanion").font.pixelSize, 20)
    }

    function test_emptyPresentationIsIdle() {
        verify(!dock.visible)
        compare(dock.height, 0)
    }

    function test_mapUpdateUsesSharedDock() {
        show({id: "map-update", kind: "info", priority: 3,
              title: "Map update available"}, {}, 76)
        compare(dock.main.id, "map-update")
        compare(dock.height, 76)
        verify(dock.hasMain)
        verify(!findChild(dock, "notificationBody").visible)
    }

    function test_largeTextFits_data() {
        return [
            {tag: "dark warning", dark: true, critical: false},
            {tag: "light warning", dark: false, critical: false},
            {tag: "dark critical companion", dark: true, critical: true},
            {tag: "light critical companion", dark: false, critical: true}
        ]
    }

    function test_largeTextFits(data) {
        dock.isDark = data.dark
        show({id: "battery", kind: "warning", priority: data.critical ? 0 : 2,
              title: data.critical ? "Battery 0: Multiple Critical Issues"
                                   : "Battery 0: Over-temperature while charging",
              body: data.critical ? "B6, B1" : ""},
             data.critical ? {kind: "nav", distance: 120, street: "Main Street"} : {},
             data.critical ? 128 : 76, data.critical ? 2 : 0)
        const title = findChild(dock, "notificationTitle")
        const body = findChild(dock, "notificationBody")
        verify(!title.truncated, "title width=" + title.width + " max=" + title.maxWidth
               + " lines=" + title.lineCount + " height=" + title.height
               + " content=" + title.contentHeight)
        verify(findChild(dock, "notificationIcon").visible)
        verify(title.width <= title.maxWidth)
        verify(title.contentHeight <= title.height + 1)
        verify(title.mapToItem(dock, 0, 0).y >= 0)
        const textBottom = body.visible ? body.mapToItem(dock, 0, body.height).y
                                        : title.mapToItem(dock, 0, title.height).y
        verify(textBottom <= dock.height - (dock.hasCompanion ? 34 : 0))
    }
}

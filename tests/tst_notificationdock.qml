import QtQuick
import QtTest
import "../qml/notifications"

TestCase {
    name: "UnifiedAttentionDock"
    when: windowShown
    width: 480
    height: 128

    QtObject {
        id: service
        property var presentation: ({main: {id: "warning", kind: "warning", priority: 2,
                                              title: "Warning", body: "Test"},
                                      companion: {id: "navigation-session", kind: "nav",
                                                  priority: 3, distance: 120, maneuverType: 5,
                                                  street: "Main Street"},
                                      criticalCount: 0, height: 92})
        property int occupiedHeight: 92
        property bool mapUpdateAvailable: false
    }

    QtObject {
        id: mapService
        property var presentation: ({main: {id: "map-update", kind: "info", priority: 3,
                                              title: "Map update available"},
                                      companion: {}, criticalCount: 0, height: 56})
        property int occupiedHeight: 56
    }

    UnifiedAttentionDock {
        id: dock
        width: 480
        service: service
    }

    UnifiedAttentionDock {
        id: mapDock
        width: 480
        service: mapService
    }

    function test_boundedWarningCompanion() {
        compare(dock.height, 92)
        verify(dock.hasCompanion)
        verify(dock.height <= 128)
    }

    function test_emptyPresentationIsIdle() {
        service.presentation = ({main: {}, companion: {}, criticalCount: 0, height: 0})
        service.occupiedHeight = 0
        verify(!dock.visible)
        compare(dock.height, 0)
    }

    function test_mapUpdateUsesSharedDock() {
        compare(mapDock.main.id, "map-update")
        compare(mapDock.height, 56)
        verify(mapDock.hasMain)
    }
}

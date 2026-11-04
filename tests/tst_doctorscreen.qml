import QtQuick
import QtTest
import ScootUITest 1.0
import "../qml/screens"

TestCase {
    name: "DoctorScreen"
    when: windowShown
    visible: true
    width: 480
    height: 480
    NativeAttentionHarness { id: harness }
    readonly property var translations: harness.translations
    QtObject {
        id: themeStore
        property bool isDark: true
        property int fontBody: 18
        property int fontCaption: 14
        property int fontTitle: 20
        property int fontHeading: 24
        property int fontMicro: 12
    }
    QtObject {
        id: doctorService
        property var boards: []
        property bool running: false
        property int starts: 0
        function start() { starts++; running = true }
    }
    QtObject {
        id: screenStore
        property int closed: 0
        function closeDoctor() { closed++ }
    }
    QtObject {
        id: menuStore
        property int resumed: 0
        function resume() { resumed++ }
    }
    QtObject {
        id: inputHandler
        signal leftTap()
        signal leftHold()
        signal rightTap()
        signal rightHold()
    }
    Component { id: screenComponent; DoctorScreen { width: 480; height: 480 } }

    function board(id, state, error, checks) {
        return {id: id, state: state, error: error, checks: checks || [], attention: 0,
            warnings: 0, unknown: 0, completedAt: "2026-11-04T12:00:00Z"}
    }
    function checks() {
        return [
            {name: "services", verdict: "attention", detail: "librescoot-vehicle.service enabled but not running", hint: "Use lsc service logs librescoot-vehicle to inspect the service."},
            {name: "link", verdict: "warning", detail: "MDB over the PPP backup link", hint: "Check the USB connection."},
            {name: "kernel", verdict: "unknown", detail: "Kernel state unavailable", hint: ""},
            {name: "ota", verdict: "ok", detail: "No update in progress", hint: ""},
            {name: "faults", verdict: "ok", detail: "No active vehicle faults", hint: ""},
            {name: "storage", verdict: "ok", detail: "2 GiB available", hint: ""}
        ]
    }
    function init() {
        doctorService.starts = 0
        doctorService.running = false
        screenStore.closed = 0
        menuStore.resumed = 0
    }
    function test_resultsAndControls_data() {
        return [{tag: "en-dark", lang: "en", dark: true}, {tag: "en-light", lang: "en", dark: false},
                {tag: "de-dark", lang: "de", dark: true}, {tag: "de-light", lang: "de", dark: false}]
    }
    function test_resultsAndControls(data) {
        translations.setLanguage(data.lang)
        themeStore.isDark = data.dark
        const dbc = board("DBC", "done", "", checks())
        dbc.attention = 1; dbc.warnings = 1; dbc.unknown = 1
        doctorService.boards = [dbc, board("MDB", "error", "unavailable", [])]
        const view = createTemporaryObject(screenComponent, this)
        verify(view !== null)
        compare(doctorService.starts, 0)
        waitForRendering(view)
        const summary = findChild(view, "doctorSummary_DBC")
        compare(summary.text, translations.doctorSummary.arg(1).arg(1).arg(1))
        verify(summary.height <= 43, "Summary must fit its header")
        compare(findChild(view, "doctorSummary_MDB").text, translations.doctorUnknown)
        const detail = findChild(view, "doctorDetail_DBC_services")
        verify(detail !== null)
        compare(detail.text, checks()[0].detail)
        compare(detail.textFormat, Text.PlainText)
        verify(!detail.truncated)
        if (typeof captureDirectory !== "undefined" && captureDirectory.length > 0)
            grabImage(view).save(captureDirectory + "/doctor-" + data.tag + ".png")
        const scroll = findChild(view, "doctorScroll")
        verify(scroll.contentHeight > scroll.height)
        inputHandler.leftTap()
        compare(scroll.contentY, 120)
        inputHandler.rightHold()
        compare(scroll.contentY, 0)
        for (let i = 0; i < 20; i++) inputHandler.leftTap()
        compare(scroll.contentY, scroll.contentHeight - scroll.height)
        inputHandler.rightTap()
        compare(doctorService.starts, 1)
        compare(scroll.contentY, 0)
        inputHandler.rightTap()
        compare(doctorService.starts, 1)
        inputHandler.leftHold()
        compare(screenStore.closed, 1)
        compare(menuStore.resumed, 1)
        view.visible = false
        inputHandler.leftHold()
        compare(screenStore.closed, 1)
    }
    function test_runningAndFailureStates() {
        doctorService.running = true
        doctorService.boards = [board("DBC", "running", "", []), board("MDB", "running", "", [])]
        const view = createTemporaryObject(screenComponent, this)
        verify(view !== null)
        compare(findChild(view, "doctorSummary_DBC").text, translations.doctorRunning)
        for (const error of ["timeout", "unsupported", "invalid", "too-large", "cancelled", "unavailable"]) {
            doctorService.boards = [board("DBC", "error", error, []), board("MDB", "done", "", checks())]
            compare(findChild(view, "doctorSummary_DBC").text, translations.doctorUnknown)
            verify(view.errorText(error).length > 0)
        }
    }
}

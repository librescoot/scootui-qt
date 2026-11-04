import QtQuick
import QtTest
import ScootUITest 1.0
import "../qml/overlays"
import "../qml/widgets/shutdown"

TestCase {
    name: "NotificationLifecycles"
    when: windowShown
    visible: true
    width: 480
    height: 480

    QtObject {
        id: themeStore
        property bool isDark: true
        property int fontBody: 18
        property int fontTitle: 20
        property int fontHeading: 24
        property int fontHero: 48
        property int fontPin: 48
        property int radiusCard: 8
        property int radiusModal: 16
    }
    QtObject {
        id: vehicleStore
        property int state: 4
        property int brakeLeft: 1
        property int brakeRight: 1
        property bool hopOnActive: false
    }
    QtObject { id: menuStore; property bool isOpen: false }
    QtObject { id: bluetoothStore; property string pinCode: "" }
    QtObject {
        id: shutdownStore
        property bool isShuttingDown: false
        signal shuttingDownChanged()
        onIsShuttingDownChanged: shuttingDownChanged()
        property bool showBlackout: false
    }
    QtObject {
        id: otaStore
        property string dbcStatus: "idle"
        property string mdbStatus: "idle"
        property string dbcUpdateVersion: "v1.2.3"
        property string mdbUpdateVersion: "v4.5.6"
        property string dbcErrorMessage: ""
        property string mdbErrorMessage: ""
        property bool isActive: dbcStatus !== "idle" || mdbStatus !== "idle"
    }
    NativeAttentionHarness { id: harness }
    readonly property var translations: harness.translations
    Component { id: pinComponent; BluetoothPinCodeOverlay {} }
    Component { id: versionComponent; VersionOverlay {} }
    Component { id: hibernationComponent; ManualHibernationOverlay {} }
    Component { id: shutdownComponent; ShutdownOverlay { width: 480; height: 480 } }

    function init() {
        vehicleStore.state = 4
        vehicleStore.brakeLeft = 1
        vehicleStore.brakeRight = 1
        vehicleStore.hopOnActive = false
        menuStore.isOpen = false
        bluetoothStore.pinCode = ""
        shutdownStore.isShuttingDown = false
        shutdownStore.showBlackout = false
        otaStore.dbcStatus = "idle"
        otaStore.mdbStatus = "idle"
        otaStore.dbcErrorMessage = ""
        otaStore.mdbErrorMessage = ""
    }
    function make(component) {
        const view = createTemporaryObject(component, this)
        verify(view !== null)
        return view
    }
    function capture(view, name) {
        waitForRendering(view)
        if (typeof captureDirectory !== "undefined" && captureDirectory.length > 0)
            grabImage(view).save(captureDirectory + "/lifecycle-" + name + ".png")
    }
    function test_pinClearsAndSeeds() {
        bluetoothStore.pinCode = "123456"
        const view = make(pinComponent)
        compare(view.currentPin, "123456")
        verify(view.visible)
        bluetoothStore.pinCode = ""
        compare(view.currentPin, "")
        verify(!view.visible)
        bluetoothStore.pinCode = "654321"
        verify(view.visible)
        vehicleStore.hopOnActive = true
        verify(!view.visible)
        bluetoothStore.pinCode = ""
        vehicleStore.hopOnActive = false
        verify(!view.visible)
    }
    function test_versionClosesOnStateOrMenu_data() {
        return [{tag: "riding", state: 5}, {tag: "standby", state: 1}, {tag: "menu", state: 4}]
    }
    function test_versionClosesOnStateOrMenu(data) {
        const view = make(versionComponent)
        vehicleStore.brakeLeft = 0
        vehicleStore.brakeRight = 0
        tryCompare(view, "visible", true, 4000)
        if (data.tag === "menu") menuStore.isOpen = true
        else vehicleStore.state = data.state
        verify(!view.visible)
        vehicleStore.state = 4
        menuStore.isOpen = false
        verify(!view.visible)
        wait(100)
        vehicleStore.brakeRight = 1
        wait(3100)
        verify(!view.visible)
    }
    function test_hibernationTracksBrakesAndMode() {
        const view = make(hibernationComponent)
        vehicleStore.state = 13
        verify(!view.countdownActive)
        vehicleStore.brakeLeft = 0
        vehicleStore.brakeRight = 0
        verify(view.countdownActive)
        tryCompare(view, "countdown", 14, 1500)
        vehicleStore.state = 15
        verify(!view.countdownActive)
        compare(view.countdown, 15)
        vehicleStore.state = 14
        verify(view.countdownActive)
        compare(view.countdown, 15)
        vehicleStore.brakeLeft = 1
        verify(!view.countdownActive)
        compare(view.countdown, 15)
    }
    function test_shutdownShowsBothBoards_data() {
        return [
            {tag: "en-dark", lang: "en", dark: true},
            {tag: "en-light", lang: "en", dark: false},
            {tag: "de-dark", lang: "de", dark: true},
            {tag: "de-light", lang: "de", dark: false}
        ]
    }
    function test_shutdownShowsBothBoards(data) {
        harness.translations.setLanguage(data.lang)
        themeStore.isDark = data.dark
        const view = make(shutdownComponent)
        otaStore.dbcStatus = "pending-reboot"
        otaStore.mdbStatus = "installing"
        shutdownStore.isShuttingDown = true
        wait(1600)
        const dbc = findChild(view, "shutdownOtaStatus_dbc")
        const mdb = findChild(view, "shutdownOtaStatus_mdb")
        verify(dbc !== null)
        verify(mdb !== null)
        verify(dbc.visible && mdb.visible)
        compare(dbc.text, translations.otaPendingReboot)
        compare(mdb.text, translations.otaInstallingUpdates)
        compare(findChild(view, "shutdownOtaVersion_dbc").text, "v1.2.3")
        compare(findChild(view, "shutdownOtaVersion_mdb").text, "v4.5.6")
        verify(dbc.mapToItem(view, 0, dbc.height).y < mdb.mapToItem(view, 0, 0).y)
        capture(view, "ota-both-" + data.tag)
        otaStore.mdbStatus = "error"
        otaStore.mdbErrorMessage = "Download verification failed"
        compare(mdb.text, translations.otaUpdateError)
        const error = findChild(view, "shutdownOtaError_mdb")
        compare(error.text, otaStore.mdbErrorMessage)
        verify(error.visible)
        capture(view, "ota-error-" + data.tag)
        otaStore.dbcStatus = "idle"
        verify(!dbc.visible)
        verify(mdb.visible)
        shutdownStore.isShuttingDown = false
        verify(!view.visible)
    }
}

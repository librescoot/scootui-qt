import QtQuick
import QtTest
import ScootUITest 1.0
import "../qml/screens"
import "../qml/screens/SimStatus.js" as SimStatus

TestCase {
    name: "SimStatus"
    when: windowShown
    visible: true
    width: 480
    height: 480

    NativeAttentionHarness { id: nativeHarness }
    readonly property var translations: nativeHarness.translations
    QtObject {
        id: themeStore
        property bool isDark: true
        property int fontBody: 18
        property int fontCaption: 14
        property int fontMicro: 10
    }
    QtObject { id: screenStore; property int systemInfoPage: 1 }
    QtObject {
        id: modemStore
        property string simState: "present"
        property string simLock: ""
        property string registration: ""
        property bool isRoaming: false
        property string operatorName: ""
        property string operatorCode: ""
    }
    Component { id: infoComponent; SystemInfoScreen { width: 480; height: 480 } }

    function init() {
        nativeHarness.translations.setLanguage("en")
        modemStore.simState = "present"
        modemStore.simLock = ""
    }

    function test_infoScreenUpdatesAndFitsBothLanguagesAndThemes() {
        const view = createTemporaryObject(infoComponent, this)
        verify(view !== null)
        for (const dark of [true, false]) {
            themeStore.isDark = dark
            modemStore.simState = "locked"
            modemStore.simLock = "sim-pin2"
            nativeHarness.translations.setLanguage("en")
            compare(view.networkRows[0].value, "Present (PIN2 only)")
            nativeHarness.translations.setLanguage("de")
            compare(view.networkRows[0].value, "Vorhanden (nur PIN2)")
            verify(waitForRendering(view))
            const value = findChild(view, "wrappedInfoValue")
            verify(value !== null)
            compare(value.text, "Vorhanden (nur PIN2)")
            verify(!value.truncated)
            verify(value.contentWidth <= value.width + 1)
            modemStore.simLock = "sim-puk2"
            compare(view.networkRows[0].value, "Vorhanden (nur PUK2)")
            modemStore.simLock = "sim-pin"
            compare(view.networkRows[0].value, "PIN erforderlich")
            modemStore.simLock = "sim-puk"
            compare(view.networkRows[0].value, "PUK erforderlich")
        }
    }

    function test_description_data() {
        return [
            { tag: "present", state: "present", lock: "", expected: "present" },
            { tag: "pin", state: "locked", lock: "sim-pin", expected: "PIN required" },
            { tag: "puk", state: "locked", lock: "sim-puk", expected: "PUK required" },
            { tag: "pin2", state: "present", lock: "sim-pin2", expected: "Present (PIN2 only)" },
            { tag: "puk2", state: "present", lock: "sim-puk2", expected: "Present (PUK2 only)" },
            { tag: "legacy-pin2", state: "locked", lock: "sim-pin2", expected: "Present (PIN2 only)" },
            { tag: "legacy-puk2", state: "locked", lock: "sim-puk2", expected: "Present (PUK2 only)" },
            { tag: "network-lock", state: "locked", lock: "ph-net-pin", expected: "Locked (ph-net-pin)" },
            { tag: "unqualified-lock", state: "locked", lock: "", expected: "Locked" },
            { tag: "none-but-locked", state: "locked", lock: "none", expected: "Locked" },
            { tag: "missing", state: "missing", lock: "sim-pin2", expected: "missing" },
            { tag: "unknown", state: "unknown", lock: "sim-pin2", expected: "unknown" },
            { tag: "inactive", state: "inactive", lock: "sim-puk2", expected: "inactive" },
            { tag: "unobserved", state: "", lock: "sim-pin2", expected: "" }
        ]
    }

    function test_description(data) {
        compare(SimStatus.describe(data.state, data.lock), data.expected)
    }

    function translated(key, fallback) {
        const value = nativeHarness.translations[key]
        return value === undefined ? fallback : value
    }

    function test_translations_data() {
        return [
            { tag: "en-pin", language: "en", lock: "sim-pin", expected: "PIN required" },
            { tag: "de-pin", language: "de", lock: "sim-pin", expected: "PIN erforderlich" },
            { tag: "de-puk", language: "de", lock: "sim-puk", expected: "PUK erforderlich" },
            { tag: "de-pin2", language: "de", lock: "sim-pin2", expected: "Vorhanden (nur PIN2)" },
            { tag: "de-puk2", language: "de", lock: "sim-puk2", expected: "Vorhanden (nur PUK2)" },
            { tag: "de-network", language: "de", lock: "ph-net-pin", expected: "Gesperrt (ph-net-pin)" }
        ]
    }

    function test_translations(data) {
        nativeHarness.translations.setLanguage(data.language)
        compare(SimStatus.describe("locked", data.lock, translated), data.expected)
    }
}

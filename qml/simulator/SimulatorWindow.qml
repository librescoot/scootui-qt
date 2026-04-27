import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: simWindow
    title: "ScootUI Simulator"
    width: 520
    height: 900
    visible: true
    color: "#1e1e1e"

    x: Screen.width / 2 - (480 + width + 20) / 2 + 480 + 20
    y: Math.max(0, Screen.height / 2 - height / 2)

    ScrollView {
        id: scroll
        anchors.fill: parent
        anchors.margins: 8
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: rootLayout
            width: scroll.availableWidth
            spacing: 8

            // ====================================================
            // TOP — daily-driver controls
            // ====================================================

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimButton {
                    text: "Screenshot"
                    color: "#607D8B"
                    onClicked: simulator.takeScreenshot()
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "ScootUI Simulator"
                    color: "#888"
                    font.pixelSize: 11
                }
            }

            SectionHeader { text: "Screen" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                ButtonGroup { id: screenGroup; exclusive: true }
                SimButton {
                    text: "Cluster"; small: true
                    ButtonGroup.group: screenGroup
                    checkable: true; checked: true
                    onClicked: screenStore.setScreen(0)
                }
                SimButton {
                    text: "Map"; small: true
                    ButtonGroup.group: screenGroup
                    checkable: true
                    onClicked: screenStore.setScreen(1)
                }
                SimButton {
                    text: "Debug"; small: true
                    ButtonGroup.group: screenGroup
                    checkable: true
                    onClicked: screenStore.setScreen(3)
                }
                SimButton {
                    text: "About"; small: true
                    ButtonGroup.group: screenGroup
                    checkable: true
                    onClicked: screenStore.setScreen(4)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                SimLabel { text: "Theme" }
                ButtonGroup { id: themeGroup; exclusive: true }
                SimButton {
                    text: "Dark"; small: true
                    ButtonGroup.group: themeGroup
                    checkable: true; checked: true
                    onClicked: simulator.setTheme("dark")
                }
                SimButton {
                    text: "Light"; small: true
                    ButtonGroup.group: themeGroup
                    checkable: true
                    onClicked: simulator.setTheme("light")
                }
                SimButton {
                    text: "Auto"; small: true
                    ButtonGroup.group: themeGroup
                    checkable: true
                    onClicked: simulator.setTheme("auto")
                }
                Item { Layout.preferredWidth: 8 }
                SimLabel { text: "Lang" }
                ButtonGroup { id: langGroup; exclusive: true }
                SimButton {
                    text: "EN"; small: true
                    ButtonGroup.group: langGroup
                    checkable: true; checked: true
                    onClicked: simulator.setLanguage("en")
                }
                SimButton {
                    text: "DE"; small: true
                    ButtonGroup.group: langGroup
                    checkable: true
                    onClicked: simulator.setLanguage("de")
                }
            }

            SectionHeader { text: "Auto-Drive" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimButton {
                    text: simulator.autoDriveActive ? "Stop" : "Start"
                    color: simulator.autoDriveActive ? "#f44336" : "#4caf50"
                    Layout.preferredWidth: 80
                    Layout.fillWidth: false
                    onClicked: {
                        if (simulator.autoDriveActive)
                            simulator.stopAutoDrive()
                        else
                            simulator.startAutoDrive(autoDriveSpeedSlider.value)
                    }
                }
                Slider {
                    id: autoDriveSpeedSlider
                    Layout.fillWidth: true
                    from: 5; to: 55; value: 25; stepSize: 1
                    onMoved: {
                        if (simulator.autoDriveActive)
                            simulator.startAutoDrive(value)
                    }
                }
                Text {
                    text: Math.round(autoDriveSpeedSlider.value) + " km/h"
                    color: simulator.autoDriveActive ? "#4caf50" : "#ccc"
                    font.pixelSize: 11
                    Layout.preferredWidth: 56
                    horizontalAlignment: Text.AlignRight
                }
            }
            Text {
                Layout.fillWidth: true
                visible: simulator.autoDriveActive
                text: "Driving at " + simulator.autoDriveSpeed.toFixed(1) + " km/h"
                color: "#4caf50"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
            }

            SectionHeader { text: "Routes" }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 4
                rowSpacing: 4
                SimButton {
                    text: "Charlottenburg → Moabit"
                    Layout.fillWidth: true
                    onClicked: simulator.loadTestRoute(1)
                }
                SimButton {
                    text: "Mitte → Moabit"
                    Layout.fillWidth: true
                    onClicked: simulator.loadTestRoute(2)
                }
                SimButton {
                    text: "Tempelhof → Friedrichshain"
                    Layout.fillWidth: true
                    onClicked: simulator.loadTestRoute(3)
                }
                SimButton {
                    text: "Short (Richard-Ermisch)"
                    Layout.fillWidth: true
                    onClicked: simulator.loadTestRoute(4)
                }
                SimButton {
                    text: "Bersarinplatz (roundabout)"
                    Layout.fillWidth: true
                    onClicked: simulator.loadTestRoute(5)
                }
                SimButton {
                    text: "Frankfurter Allee (U-turn)"
                    Layout.fillWidth: true
                    onClicked: simulator.loadTestRoute(6)
                }
                SimButton {
                    text: "Clear Route"
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    color: "#f44336"
                    onClicked: {
                        if (typeof navigationService !== "undefined")
                            navigationService.clearNavigation()
                    }
                }
            }

            // ====================================================
            // MIDDLE — vehicle presets, overrides, common injection
            // ====================================================

            SectionHeader { text: "Vehicle Presets" }
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 4
                rowSpacing: 4
                SimButton { text: "Parked"; onClicked: simulator.loadPreset("parked") }
                SimButton { text: "Ready"; onClicked: simulator.loadPreset("ready") }
                SimButton { text: "Driving"; onClicked: simulator.loadPreset("driving") }
                SimButton { text: "Fast"; onClicked: simulator.loadPreset("driving-fast") }
                SimButton { text: "Low Batt"; color: "#ff6b35"; onClicked: simulator.loadPreset("low-battery") }
                SimButton { text: "Charging"; color: "#4caf50"; onClicked: simulator.loadPreset("charging") }
                SimButton { text: "No GPS"; onClicked: simulator.loadPreset("no-gps") }
                SimButton { text: "Offline"; onClicked: simulator.loadPreset("offline") }
                SimButton { text: "1 Battery"; onClicked: simulator.loadPreset("single-battery") }
            }

            SectionHeader { text: "Overrides" }
            // Order matches the dashboard's display order: clock at top of
            // screen, trip metrics in the bottom status bar (Duration · Avg ·
            // Trip · Total).

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Clock" }
                TextField {
                    id: clockOverrideField
                    Layout.fillWidth: true
                    placeholderText: "HH:mm (empty = real)"
                    text: simulator.clockOverride
                    color: "white"
                    font.pixelSize: 12
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: simulator.clockOverride = text
                }
                SimButton {
                    text: "Clear"; small: true; color: "#666"
                    onClicked: { clockOverrideField.text = ""; simulator.clockOverride = "" }
                }
            }

            // Trip block — edit any two of {Duration, Avg, Trip distance};
            // the third recomputes. Most-recently-edited two are kept; the
            // stale one is overwritten.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                property var lastTwo: ["dur", "avg"]

                function noteEdit(which) {
                    if (lastTwo[0] === which) {
                        // already current; nothing to reorder
                    } else if (lastTwo.indexOf(which) >= 0) {
                        lastTwo = [which, lastTwo[0]]
                    } else {
                        lastTwo = [which, lastTwo[0]]
                    }
                    recompute()
                }

                function recompute() {
                    var dist = parseFloat(tripDistField.text)
                    var dur  = parseFloat(tripDurField.text)
                    var avg  = parseFloat(tripAvgField.text)
                    if (isNaN(dist)) dist = 0
                    if (isNaN(dur))  dur  = 0
                    if (isNaN(avg))  avg  = 0
                    var stale = ["dur", "avg", "dist"].find(function(f){ return lastTwo.indexOf(f) < 0 })
                    if (stale === "dist") {
                        dist = avg * (dur / 3600.0)
                        tripDistField.text = dist.toFixed(2)
                    } else if (stale === "dur") {
                        if (avg > 0) {
                            dur = (dist / avg) * 3600.0
                            tripDurField.text = Math.round(dur).toString()
                        }
                    } else if (stale === "avg") {
                        if (dur > 0) {
                            avg = dist / (dur / 3600.0)
                            tripAvgField.text = avg.toFixed(1)
                        }
                    }
                    if (typeof tripStore !== "undefined")
                        tripStore.setOverride(dist, Math.round(dur), avg)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SimLabel { text: "Duration" }
                    TextField {
                        id: tripDurField
                        Layout.preferredWidth: 80
                        text: "1830"
                        placeholderText: "s"
                        color: "white"; font.pixelSize: 11
                        background: Rectangle { color: "#333"; radius: 3 }
                        onEditingFinished: parent.parent.noteEdit("dur")
                    }
                    Text { text: "s"; color: "#999"; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SimLabel { text: "Avg" }
                    TextField {
                        id: tripAvgField
                        Layout.preferredWidth: 80
                        text: "24.2"
                        placeholderText: "km/h"
                        color: "white"; font.pixelSize: 11
                        background: Rectangle { color: "#333"; radius: 3 }
                        onEditingFinished: parent.parent.noteEdit("avg")
                    }
                    Text { text: "km/h"; color: "#999"; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SimLabel { text: "Trip" }
                    TextField {
                        id: tripDistField
                        Layout.preferredWidth: 80
                        text: "12.3"
                        placeholderText: "km"
                        color: "white"; font.pixelSize: 11
                        background: Rectangle { color: "#333"; radius: 3 }
                        onEditingFinished: parent.parent.noteEdit("dist")
                    }
                    Text { text: "km"; color: "#999"; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                    SimButton {
                        text: "Clear"; small: true; color: "#666"
                        onClicked: {
                            if (typeof tripStore !== "undefined")
                                tripStore.clearOverride()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Total" }
                TextField {
                    id: odometerOverrideField
                    Layout.preferredWidth: 80
                    text: "0.0"
                    placeholderText: "km"
                    color: "white"; font.pixelSize: 11
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: {
                        var v = parseFloat(text)
                        if (!isNaN(v)) simulator.setOdometer(v)
                    }
                    Component.onCompleted: simulator.setOdometer(0)
                }
                Text { text: "km"; color: "#999"; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
            }

            SectionHeader { text: "Engine" }
            SimSliderRow {
                label: "Speed"
                from: 0; to: 55; value: 0; unit: "km/h"; decimals: 0
                onMoved: function(v) { simulator.setSpeed(v); simulator.setGpsSpeed(v) }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                SimLabel { text: "Main pwr" }
                SimButton { text: "On"; small: true
                    onClicked: simulator.setMainPower(true) }
                SimButton { text: "Off"; small: true; color: "#f44336"
                    onClicked: simulator.setMainPower(false) }
                Item { Layout.preferredWidth: 4 }
                SimLabel { text: "Motor" }
                SimButton { text: "On"; small: true
                    onClicked: simulator.setMotorPower(true) }
                SimButton { text: "Off"; small: true; color: "#f44336"
                    onClicked: simulator.setMotorPower(false) }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                SimLabel { text: "KERS" }
                SimButton { text: "On"; small: true
                    onClicked: simulator.setKers(true) }
                SimButton { text: "Off"; small: true; color: "#f44336"
                    onClicked: simulator.setKers(false) }
                Item { Layout.preferredWidth: 4 }
                SimLabel { text: "Throttle" }
                SimButton { text: "On"; small: true
                    onClicked: simulator.setThrottle(true) }
                SimButton { text: "Off"; small: true; color: "#f44336"
                    onClicked: simulator.setThrottle(false) }
            }

            SectionHeader { text: "Vehicle State" }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "State" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["parked", "ready-to-drive", "stand-by", "off",
                            "shutting-down", "booting", "hibernating",
                            "waiting-hibernation", "updating"]
                    currentIndex: 0
                    onActivated: simulator.setVehicleState(currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                    palette.highlight: "#2196F3"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Kickstand" }
                Switch {
                    id: kickstandSwitch
                    checked: true
                    text: checked ? "Down" : "Up"
                    palette.highlight: "#2196F3"
                    onToggled: simulator.setKickstand(checked ? "down" : "up")
                }
                Item { Layout.preferredWidth: 8 }
                SimLabel { text: "Blinkers" }
                ButtonGroup { id: blinkerGroup; exclusive: true }
                SimButton {
                    text: "Off"; small: true
                    ButtonGroup.group: blinkerGroup
                    checkable: true; checked: true
                    onClicked: simulator.setBlinkerState("off")
                }
                SimButton {
                    text: "L"; small: true
                    ButtonGroup.group: blinkerGroup
                    checkable: true
                    onClicked: simulator.setBlinkerState("left")
                }
                SimButton {
                    text: "R"; small: true
                    ButtonGroup.group: blinkerGroup
                    checkable: true
                    onClicked: simulator.setBlinkerState("right")
                }
                SimButton {
                    text: "Both"; small: true
                    ButtonGroup.group: blinkerGroup
                    checkable: true
                    onClicked: simulator.setBlinkerState("both")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Brakes" }
                SimButton {
                    text: "Left"; small: true
                    onPressed: simulator.setBrakeLeft(true)
                    onReleased: simulator.setBrakeLeft(false)
                }
                SimButton {
                    text: "Right"; small: true
                    onPressed: simulator.setBrakeRight(true)
                    onReleased: simulator.setBrakeRight(false)
                }
                Item { Layout.preferredWidth: 8 }
                SimLabel { text: "Buttons" }
                SimButton {
                    text: "S-Box"; small: true
                    onPressed: simulator.setSeatboxButton(true)
                    onReleased: simulator.setSeatboxButton(false)
                }
                SimButton {
                    text: "Horn"; small: true; color: "#ff9800"
                    onPressed: simulator.setHornButton(true)
                    onReleased: simulator.setHornButton(false)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                SimLabel { text: "Locks" }
                SimButton {
                    text: "S-Box Open"; small: true; onClicked: simulator.setSeatboxLock("open")
                }
                SimButton {
                    text: "S-Box Close"; small: true; onClicked: simulator.setSeatboxLock("closed")
                }
                SimButton {
                    text: "H-Bar Lock"; small: true; onClicked: simulator.setHandlebarLock("locked")
                }
                SimButton {
                    text: "H-Bar Unlock"; small: true; onClicked: simulator.setHandlebarLock("unlocked")
                }
            }

            SectionHeader { text: "GPS" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Freeze" }
                Switch {
                    checked: simulator.gpsFrozen
                    onToggled: {
                        simulator.gpsFrozen = checked
                        if (typeof mapService !== "undefined")
                            mapService.deadReckoningPaused = checked
                    }
                    palette.highlight: "#2196F3"
                }
                SimLabel { text: "State" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["fix-established", "searching", "off", "error"]
                    currentIndex: 0
                    onActivated: simulator.setGpsState(currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Lat" }
                TextField {
                    id: latField
                    Layout.fillWidth: true
                    text: "52.520008"
                    color: "white"; font.pixelSize: 12
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: simulator.setGpsPosition(
                        parseFloat(text), parseFloat(lngField.text))
                }
                SimLabel { text: "Lng" }
                TextField {
                    id: lngField
                    Layout.fillWidth: true
                    text: "13.404954"
                    color: "white"; font.pixelSize: 12
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: simulator.setGpsPosition(
                        parseFloat(latField.text), parseFloat(text))
                }
            }
            SimSliderRow {
                label: "Course"
                from: 0; to: 359; value: 0; unit: "°"; decimals: 0
                onMoved: function(v) { simulator.setGpsCourse(v) }
            }

            SectionHeader { text: "Battery" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "B0" }
                Switch {
                    checked: true
                    palette.highlight: "#2196F3"
                    onToggled: simulator.setBatteryPresent(0, checked)
                }
                ComboBox {
                    Layout.preferredWidth: 84
                    model: ["active", "idle", "asleep", "unknown"]
                    currentIndex: 0
                    onActivated: simulator.setBatteryState(0, currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                Slider {
                    id: b0Slider
                    Layout.fillWidth: true
                    from: 0; to: 100; value: 80; stepSize: 1
                    onMoved: simulator.setBatteryCharge(0, Math.round(value))
                }
                Text {
                    text: Math.round(b0Slider.value) + "%"
                    color: "#ccc"; font.pixelSize: 11
                    Layout.preferredWidth: 36
                    horizontalAlignment: Text.AlignRight
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "B1" }
                Switch {
                    checked: true
                    palette.highlight: "#2196F3"
                    onToggled: simulator.setBatteryPresent(1, checked)
                }
                ComboBox {
                    Layout.preferredWidth: 84
                    model: ["active", "idle", "asleep", "unknown"]
                    currentIndex: 1
                    onActivated: simulator.setBatteryState(1, currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                Slider {
                    id: b1Slider
                    Layout.fillWidth: true
                    from: 0; to: 100; value: 75; stepSize: 1
                    onMoved: simulator.setBatteryCharge(1, Math.round(value))
                }
                Text {
                    text: Math.round(b1Slider.value) + "%"
                    color: "#ccc"; font.pixelSize: 11
                    Layout.preferredWidth: 36
                    horizontalAlignment: Text.AlignRight
                }
            }

            SectionHeader { text: "OTA" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "MDB" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["idle", "downloading", "preparing", "installing",
                            "pending-reboot", "error"]
                    currentIndex: 0
                    onActivated: simulator.setOtaStatus("mdb", currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                SimLabel { text: "DBC" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["idle", "downloading", "preparing", "installing",
                            "pending-reboot", "error"]
                    currentIndex: 0
                    onActivated: simulator.setOtaStatus("dbc", currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Method" }
                ButtonGroup { id: otaMethodMdbGroup; exclusive: true }
                SimButton {
                    text: "MDB delta"; small: true
                    ButtonGroup.group: otaMethodMdbGroup
                    checkable: true
                    onClicked: simulator.setOtaUpdateMethod("mdb", "delta")
                }
                SimButton {
                    text: "MDB full"; small: true
                    ButtonGroup.group: otaMethodMdbGroup
                    checkable: true
                    onClicked: simulator.setOtaUpdateMethod("mdb", "full")
                }
                Item { Layout.preferredWidth: 8 }
                ButtonGroup { id: otaMethodDbcGroup; exclusive: true }
                SimButton {
                    text: "DBC delta"; small: true
                    ButtonGroup.group: otaMethodDbcGroup
                    checkable: true
                    onClicked: simulator.setOtaUpdateMethod("dbc", "delta")
                }
                SimButton {
                    text: "DBC full"; small: true
                    ButtonGroup.group: otaMethodDbcGroup
                    checkable: true
                    onClicked: simulator.setOtaUpdateMethod("dbc", "full")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "DL%" }
                Slider {
                    id: otaDlSlider
                    Layout.fillWidth: true
                    from: 0; to: 100; value: 0; stepSize: 1
                    onMoved: {
                        var v = Math.round(value)
                        simulator.setOtaDownloadProgress("mdb", v)
                        simulator.setOtaDownloadProgress("dbc", v)
                    }
                }
                Text { text: Math.round(otaDlSlider.value) + "%"; color: "#ccc"; font.pixelSize: 11; Layout.preferredWidth: 36 }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Inst%" }
                Slider {
                    id: otaInstSlider
                    Layout.fillWidth: true
                    from: 0; to: 100; value: 0; stepSize: 1
                    onMoved: {
                        var v = Math.round(value)
                        simulator.setOtaInstallProgress("mdb", v)
                        simulator.setOtaInstallProgress("dbc", v)
                    }
                }
                Text { text: Math.round(otaInstSlider.value) + "%"; color: "#ccc"; font.pixelSize: 11; Layout.preferredWidth: 36 }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimButton {
                    text: "Set Versions"; small: true
                    onClicked: {
                        simulator.setOtaUpdateVersion("mdb", "v0.99.0")
                        simulator.setOtaUpdateVersion("dbc", "v0.99.0")
                    }
                }
                ComboBox {
                    id: otaErrorCombo
                    Layout.preferredWidth: 130
                    model: ["file-not-found", "invalid-file", "download-failed",
                            "install-failed", "reboot-failed", "delta-failed"]
                    currentIndex: 2
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                SimButton {
                    text: "Trigger Error (MDB)"; small: true; color: "#ff9800"
                    onClicked: {
                        simulator.setOtaError("mdb", otaErrorCombo.currentText)
                        simulator.setOtaErrorMessage("mdb", "Simulated " + otaErrorCombo.currentText)
                    }
                }
                SimButton {
                    text: "Reset"; small: true; color: "#666"
                    onClicked: simulator.clearOta()
                }
            }

            SectionHeader { text: "Connectivity" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Modem" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["connected", "disconnected", "off"]
                    currentIndex: 0
                    onActivated: simulator.setModemState(currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                SimLabel { text: "Cloud" }
                SimButton {
                    text: "On"; small: true; onClicked: simulator.setCloudConnection("connected")
                }
                SimButton {
                    text: "Off"; small: true; color: "#f44336"
                    onClicked: simulator.setCloudConnection("disconnected")
                }
            }
            SimSliderRow {
                label: "Signal"
                from: 0; to: 100; value: 75; unit: "%"; decimals: 0
                onMoved: function(v) { simulator.setSignalQuality(Math.round(v)) }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                SimLabel { text: "Tech" }
                ButtonGroup { id: techGroup; exclusive: true }
                Repeater {
                    model: ["LTE", "UMTS", "EDGE", "GSM"]
                    SimButton {
                        text: modelData; small: true
                        ButtonGroup.group: techGroup
                        checkable: true; checked: index === 0
                        onClicked: simulator.setAccessTech(modelData)
                    }
                }
                Item { Layout.preferredWidth: 8 }
                SimLabel { text: "BT" }
                SimButton {
                    text: "On"; small: true
                    onClicked: simulator.setBluetoothStatus("connected")
                }
                SimButton {
                    text: "Off"; small: true; color: "#f44336"
                    onClicked: simulator.setBluetoothStatus("disconnected")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                SimLabel { text: "USB" }
                SimButton {
                    text: "Disconnect"; small: true; color: "#f44336"
                    onClicked: connectionStore.simulateUsbDisconnect(true)
                }
                SimButton {
                    text: "Reconnect"; small: true; color: "#4caf50"
                    onClicked: connectionStore.simulateUsbDisconnect(false)
                }
                Item { Layout.preferredWidth: 8 }
                SimLabel { text: "UMS" }
                SimButton {
                    text: "Activate"; small: true; color: "#2196F3"
                    onClicked: { simulator.setUsbStatus("active"); simulator.setUsbMode("ums") }
                }
                SimButton {
                    text: "Exit"; small: true; color: "#f44336"
                    onClicked: { simulator.setUsbStatus("idle"); simulator.setUsbMode("normal") }
                }
            }

            // ====================================================
            // BOTTOM — collapsible long-tail
            // ====================================================

            CollapsibleSection {
                title: "Auto-Lock"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Trigger" }
                        SimButton { text: "60s"; small: true; onClicked: simulator.setAutoStandbyDeadline(60) }
                        SimButton { text: "30s"; small: true; onClicked: simulator.setAutoStandbyDeadline(30) }
                        SimButton { text: "10s"; small: true; onClicked: simulator.setAutoStandbyDeadline(10) }
                        SimButton { text: "Clear"; small: true; color: "#f44336"
                            onClicked: simulator.clearAutoStandbyDeadline() }
                    }
                    SimSliderRow {
                        label: "Timeout"
                        from: 0; to: 1800; value: 900; unit: "s"; decimals: 0
                        onMoved: function(v) { simulator.setAutoStandbySetting(Math.round(v)) }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: typeof autoStandbyStore !== "undefined"
                                 && autoStandbyStore.remainingSeconds > 0
                        text: "Remaining: " + (typeof autoStandbyStore !== "undefined"
                              ? autoStandbyStore.remainingSeconds : 0) + "s"
                        color: typeof autoStandbyStore !== "undefined"
                               && autoStandbyStore.remainingSeconds <= 60 ? "#FF9800" : "#4caf50"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            CollapsibleSection {
                title: "Engine (extras)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SimSliderRow {
                        label: "Eng T"
                        from: -10; to: 120; value: 25; unit: "°C"; decimals: 0
                        onMoved: function(v) { simulator.setEngineTemperature(v) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimSliderRow {
                            Layout.fillWidth: true
                            label: "Ambient"
                            from: -20; to: 50; value: 18.5; unit: "°C"; decimals: 1
                            onMoved: function(v) { simulator.setAmbientTemperature(v) }
                        }
                        SimButton {
                            text: "Clear"; small: true
                            onClicked: simulator.clearAmbientTemperature()
                        }
                    }
                    SimSliderRow {
                        label: "Motor I"
                        from: -10000; to: 80000; value: 0; unit: "mA"; decimals: 0
                        onMoved: function(v) { simulator.setMotorCurrent(v) }
                    }
                    SimSliderRow {
                        label: "Motor V"
                        from: 0; to: 60000; value: 54000; unit: "mV"; decimals: 0
                        onMoved: function(v) { simulator.setMotorVoltage(v) }
                    }
                    SimSliderRow {
                        label: "RPM"
                        from: 0; to: 8000; value: 0; unit: ""; decimals: 0
                        onMoved: function(v) { simulator.setRpm(v) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Easter" }
                        CheckBox {
                            checked: typeof odometerMilestoneService !== "undefined"
                                     && odometerMilestoneService.easterEggsEnabled
                            onToggled: {
                                if (typeof odometerMilestoneService !== "undefined")
                                    odometerMilestoneService.easterEggsEnabled = checked
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "666, 1024, 1234.5, 1337, 3133.7, 8008.5, 9999.9"
                            color: "#888"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Fault" }
                        TextField {
                            id: faultCodeField
                            Layout.preferredWidth: 64
                            text: "0"; placeholderText: "code"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        TextField {
                            id: faultDescField
                            Layout.fillWidth: true
                            placeholderText: "description"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        SimButton {
                            text: "Set"; small: true
                            onClicked: simulator.setEngineFault(parseInt(faultCodeField.text) || 0,
                                                                faultDescField.text)
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "Battery (deep)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: 2
                        delegate: ColumnLayout {
                            id: slotCol
                            required property int index
                            Layout.fillWidth: true
                            spacing: 4
                            property int slot: index
                            Text {
                                text: "Slot " + slotCol.slot
                                color: "#ccc"; font.pixelSize: 11; font.bold: true
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                SimLabel { text: "V (mV)" }
                                TextField {
                                    Layout.preferredWidth: 80
                                    text: "54000"
                                    color: "white"; font.pixelSize: 11
                                    background: Rectangle { color: "#333"; radius: 3 }
                                    onEditingFinished: simulator.setBatteryVoltage(slotCol.slot,
                                                                                   parseInt(text) || 0)
                                }
                                SimLabel { text: "I (mA)" }
                                TextField {
                                    Layout.preferredWidth: 80
                                    text: "0"
                                    color: "white"; font.pixelSize: 11
                                    background: Rectangle { color: "#333"; radius: 3 }
                                    onEditingFinished: simulator.setBatteryCurrent(slotCol.slot,
                                                                                   parseInt(text) || 0)
                                }
                                SimSliderRow {
                                    Layout.fillWidth: true
                                    label: "T"
                                    from: -20; to: 60; value: 25; unit: "°C"; decimals: 0
                                    onMoved: function(v) {
                                        simulator.setBatteryTemperature(slotCol.slot, Math.round(v))
                                    }
                                }
                            }
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "Bluetooth (deep)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "MAC" }
                        TextField {
                            id: btMacField
                            Layout.fillWidth: true
                            text: "AA:BB:CC:DD:EE:FF"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setBluetoothMac(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "PIN" }
                        TextField {
                            id: btPinField
                            Layout.fillWidth: true
                            placeholderText: "(empty = no pairing)"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setBluetoothPin(text)
                        }
                        SimButton {
                            text: "123456"; small: true
                            onClicked: { btPinField.text = "123456"; simulator.setBluetoothPin("123456") }
                        }
                        SimButton {
                            text: "Clear"; small: true; color: "#666"
                            onClicked: { btPinField.text = ""; simulator.setBluetoothPin("") }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Health" }
                        SimButton { text: "ok"; small: true; onClicked: simulator.setBluetoothServiceHealth("ok") }
                        SimButton { text: "warn"; small: true; onClicked: simulator.setBluetoothServiceHealth("warn") }
                        SimButton { text: "error"; small: true; color: "#f44336"
                            onClicked: simulator.setBluetoothServiceHealth("error") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Error" }
                        TextField {
                            id: btErrField
                            Layout.fillWidth: true
                            placeholderText: "service-error string"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setBluetoothServiceError(text)
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "Internet (deep)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "IP" }
                        TextField {
                            Layout.fillWidth: true
                            text: "10.0.0.42"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setIpAddress(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "IMEI" }
                        TextField {
                            Layout.fillWidth: true
                            text: "351756051523999"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setSimImei(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "IMSI" }
                        TextField {
                            Layout.fillWidth: true
                            text: "262011000000000"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setSimImsi(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "ICCID" }
                        TextField {
                            Layout.fillWidth: true
                            text: "8949010000000000000"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setSimIccid(text)
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "GPS (deep)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SimSliderRow {
                        label: "Alt"
                        from: -50; to: 1500; value: 34; unit: "m"; decimals: 0
                        onMoved: function(v) { simulator.setGpsAltitude(v) }
                    }
                    SimSliderRow {
                        label: "Hdop"
                        from: 0; to: 25; value: 1.0; unit: ""; decimals: 1
                        onMoved: function(v) { simulator.setGpsHdop(v) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Sats" }
                        Text { text: "used"; color: "#999"; font.pixelSize: 11 }
                        TextField {
                            id: satsUsedField
                            Layout.preferredWidth: 48
                            text: "8"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        Text { text: "vis"; color: "#999"; font.pixelSize: 11 }
                        TextField {
                            id: satsVisField
                            Layout.preferredWidth: 48
                            text: "12"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        SimButton {
                            text: "Set"; small: true
                            onClicked: simulator.setGpsSatellites(parseInt(satsUsedField.text) || 0,
                                                                  parseInt(satsVisField.text) || 0)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Field" }
                        TextField {
                            id: gpsFieldName
                            Layout.preferredWidth: 100
                            placeholderText: "field"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        TextField {
                            id: gpsFieldValue
                            Layout.fillWidth: true
                            placeholderText: "value"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        SimButton {
                            text: "Set"; small: true
                            onClicked: simulator.setGpsField(gpsFieldName.text, gpsFieldValue.text)
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "Speed Limit / Road"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Limit" }
                        TextField {
                            Layout.preferredWidth: 80
                            text: "50"; placeholderText: "km/h"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setSpeedLimit(text)
                        }
                        SimLabel { text: "Type" }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["", "residential", "secondary", "primary", "motorway",
                                    "tertiary", "service", "footway"]
                            onActivated: simulator.setRoadType(currentText)
                            palette.button: "#333"; palette.buttonText: "white"
                            palette.window: "#333"; palette.windowText: "white"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Name" }
                        TextField {
                            Layout.fillWidth: true
                            text: "Alexanderplatz"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setRoadName(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Refs" }
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "B 96"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setRoadRefs(text)
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "Settings (visibility / alarm / blinker)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: [
                            {key: "dashboard.show-gps", label: "GPS icon"},
                            {key: "dashboard.show-bluetooth", label: "BT icon"},
                            {key: "dashboard.show-cloud", label: "Cloud icon"},
                            {key: "dashboard.show-internet", label: "Internet icon"},
                            {key: "dashboard.show-clock", label: "Clock"},
                            {key: "dashboard.show-temperature", label: "Temp"}
                        ]
                        delegate: RowLayout {
                            id: visRow
                            Layout.fillWidth: true
                            spacing: 4
                            required property var modelData
                            property string settingKey: modelData.key
                            Text {
                                text: visRow.modelData.label
                                color: "#999"; font.pixelSize: 11
                                Layout.preferredWidth: 90
                            }
                            ButtonGroup { id: visGroup }
                            Repeater {
                                model: ["always", "active-or-error", "error", "never"]
                                SimButton {
                                    required property string modelData
                                    text: modelData
                                    small: true
                                    ButtonGroup.group: visGroup
                                    checkable: true
                                    onClicked: simulator.setSetting(visRow.settingKey, modelData)
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Blinker" }
                        SimButton { text: "Icon"; small: true
                            onClicked: simulator.setSetting("dashboard.blinker-style", "icon") }
                        SimButton { text: "Overlay"; small: true
                            onClicked: simulator.setSetting("dashboard.blinker-style", "overlay") }
                        Item { Layout.preferredWidth: 8 }
                        SimLabel { text: "DBC LED" }
                        SimButton { text: "On"; small: true
                            onClicked: simulator.setSetting("scooter.dbc-blinker-led", "enabled") }
                        SimButton { text: "Off"; small: true
                            onClicked: simulator.setSetting("scooter.dbc-blinker-led", "disabled") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Alarm" }
                        SimButton { text: "Enable"; small: true; color: "#4caf50"
                            onClicked: simulator.setSetting("alarm.enabled", "true") }
                        SimButton { text: "Disable"; small: true; color: "#f44336"
                            onClicked: simulator.setSetting("alarm.enabled", "false") }
                        SimButton { text: "Honk"; small: true
                            onClicked: simulator.setSetting("alarm.honk", "true") }
                        SimButton { text: "10s"; small: true
                            onClicked: simulator.setSetting("alarm.duration", "10") }
                        SimButton { text: "30s"; small: true
                            onClicked: simulator.setSetting("alarm.duration", "30") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Battery" }
                        SimButton { text: "Single"; small: true
                            onClicked: simulator.setDualBattery(false) }
                        SimButton { text: "Dual"; small: true
                            onClicked: simulator.setDualBattery(true) }
                        Item { Layout.preferredWidth: 8 }
                        SimLabel { text: "Map" }
                        SimButton { text: "Online"; small: true
                            onClicked: simulator.setSetting("dashboard.map.type", "online") }
                        SimButton { text: "Offline"; small: true
                            onClicked: simulator.setSetting("dashboard.map.type", "offline") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Traffic" }
                        Switch {
                            checked: false
                            palette.highlight: "#2196F3"
                            onToggled: simulator.setTrafficOverlay(checked)
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "Vehicle (deep)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "H-Bar pos" }
                        SimButton { text: "On-place"; small: true
                            onClicked: simulator.setHandlebarPosition("on-place") }
                        SimButton { text: "Off-place"; small: true
                            onClicked: simulator.setHandlebarPosition("off-place") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Drive" }
                        SimButton { text: "Unable"; small: true; color: "#f44336"
                            onClicked: simulator.setUnableToDrive(true) }
                        SimButton { text: "Able"; small: true
                            onClicked: simulator.setUnableToDrive(false) }
                        Item { Layout.preferredWidth: 8 }
                        SimLabel { text: "Hop-on" }
                        SimButton { text: "Active"; small: true
                            onClicked: simulator.setHopOnActive(true) }
                        SimButton { text: "Idle"; small: true
                            onClicked: simulator.setHopOnActive(false) }
                    }
                }
            }

            CollapsibleSection {
                title: "USB / UMS (deep)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Step" }
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "step name"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setUsbStep(text)
                        }
                    }
                    SimSliderRow {
                        label: "Progress"
                        from: 0; to: 100; value: 0; unit: "%"; decimals: 0
                        onMoved: function(v) { simulator.setUsbProgress(Math.round(v)) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Detail" }
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "detail line"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setUsbDetail(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Log" }
                        TextField {
                            id: umsLogField
                            Layout.fillWidth: true
                            placeholderText: "log line"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        SimButton {
                            text: "Push"; small: true
                            onClicked: { simulator.pushUmsLog(umsLogField.text); umsLogField.text = "" }
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "CB / Aux Battery"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text { text: "CB battery"; color: "#ccc"; font.pixelSize: 11; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Present" }
                        Switch {
                            checked: true
                            palette.highlight: "#2196F3"
                            onToggled: simulator.setCbBatteryField("present", checked ? "true" : "false")
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Charge" }
                        TextField {
                            Layout.preferredWidth: 64
                            text: "95"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setCbBatteryField("charge", text)
                        }
                        SimLabel { text: "Temp" }
                        TextField {
                            Layout.preferredWidth: 64
                            text: "23"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setCbBatteryField("temperature", text)
                        }
                    }
                    Text { text: "Aux battery"; color: "#ccc"; font.pixelSize: 11; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Voltage" }
                        TextField {
                            Layout.preferredWidth: 80
                            text: "12500"; placeholderText: "mV"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setAuxBatteryField("voltage", text)
                        }
                        SimLabel { text: "Charge" }
                        TextField {
                            Layout.preferredWidth: 64
                            text: "100"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setAuxBatteryField("charge", text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Status" }
                        SimButton { text: "not-charging"; small: true
                            onClicked: simulator.setAuxBatteryField("charge-status", "not-charging") }
                        SimButton { text: "bulk"; small: true
                            onClicked: simulator.setAuxBatteryField("charge-status", "bulk-charge") }
                        SimButton { text: "absorb"; small: true
                            onClicked: simulator.setAuxBatteryField("charge-status", "absorption-charge") }
                        SimButton { text: "float"; small: true
                            onClicked: simulator.setAuxBatteryField("charge-status", "float-charge") }
                    }
                }
            }

            CollapsibleSection {
                title: "Dashboard / Theme service"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Debug" }
                        SimButton { text: "Off"; small: true
                            onClicked: simulator.setDebugOverlay("off") }
                        SimButton { text: "Overlay"; small: true
                            onClicked: simulator.setDebugOverlay("overlay") }
                    }
                    SimSliderRow {
                        label: "Bright"
                        from: 0; to: 1500; value: 200; unit: "lx"; decimals: 0
                        onMoved: function(v) { simulator.setBrightness(v) }
                    }
                }
            }

            CollapsibleSection {
                title: "System / Versions"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "MDB" }
                        TextField {
                            Layout.fillWidth: true
                            text: "v1.0.0-sim"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setMdbVersion(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "DBC" }
                        TextField {
                            Layout.fillWidth: true
                            text: "v1.0.0-sim"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setDbcVersion(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "nRF" }
                        TextField {
                            Layout.fillWidth: true
                            text: "v0.5.0-sim"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: simulator.setSystemField("nrf-fw-version", text)
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "Raw Redis Injection"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        text: "Direct (channel, field) hash setter — escape hatch for anything not exposed above."
                        color: "#888"; font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        TextField {
                            id: rawChannel
                            Layout.preferredWidth: 100
                            placeholderText: "channel"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        TextField {
                            id: rawField
                            Layout.preferredWidth: 110
                            placeholderText: "field"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        TextField {
                            id: rawValue
                            Layout.fillWidth: true
                            placeholderText: "value"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        SimButton {
                            text: "Set"; small: true; color: "#2196F3"
                            onClicked: simulator.setRaw(rawChannel.text, rawField.text, rawValue.text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        TextField {
                            id: pubChannel
                            Layout.preferredWidth: 110
                            placeholderText: "channel"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        TextField {
                            id: pubMessage
                            Layout.fillWidth: true
                            placeholderText: "pub/sub message"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                        }
                        SimButton {
                            text: "Publish"; small: true
                            onClicked: simulator.publishMessage(pubChannel.text, pubMessage.text)
                        }
                        SimButton {
                            text: "LPush"; small: true
                            onClicked: simulator.pushList(pubChannel.text, pubMessage.text)
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }

    // ====================================================
    // Reusable components
    // ====================================================

    component SectionHeader: Rectangle {
        property alias text: headerText.text
        Layout.fillWidth: true
        Layout.topMargin: 6
        Layout.preferredHeight: 24
        color: "#333"
        radius: 3
        Text {
            id: headerText
            anchors.centerIn: parent
            color: "#2196F3"
            font.pixelSize: 12
            font.bold: true
        }
    }

    component CollapsibleSection: ColumnLayout {
        id: section
        property string title: ""
        property bool expanded: false
        default property alias contentItem: contentHolder.data
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 4

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: "#2a2a2a"
            radius: 3
            border.color: "#444"; border.width: 1
            Text {
                anchors.left: parent.left; anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: (section.expanded ? "▼ " : "▶ ") + section.title
                color: "#bbb"
                font.pixelSize: 11
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: section.expanded = !section.expanded
            }
        }

        ColumnLayout {
            id: contentHolder
            Layout.fillWidth: true
            Layout.leftMargin: 4
            spacing: 6
            visible: section.expanded
        }
    }

    component SimLabel: Text {
        color: "#999"
        font.pixelSize: 12
        Layout.preferredWidth: 56
    }

    component SimButton: Button {
        property bool small: false
        property color color: "#555"
        Layout.fillWidth: !small
        Layout.preferredWidth: small ? 56 : -1
        Layout.preferredHeight: small ? 26 : 30
        font.pixelSize: small ? 11 : 12
        background: Rectangle {
            color: parent.down ? Qt.lighter(parent.color, 1.3)
                   : parent.checked ? "#2196F3"
                   : parent.color
            radius: 4
        }
        contentItem: Text {
            text: parent.text
            color: "white"
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component SimSliderRow: RowLayout {
        property alias label: sliderLabel.text
        property alias from: slider.from
        property alias to: slider.to
        property alias value: slider.value
        property string unit: ""
        property int decimals: 1
        signal moved(real value)

        Layout.fillWidth: true
        spacing: 6
        SimLabel { id: sliderLabel }
        Slider {
            id: slider
            Layout.fillWidth: true
            stepSize: decimals === 0 ? 1 : 0.1
            onMoved: parent.moved(value)
        }
        Text {
            text: (decimals === 0 ? Math.round(slider.value) : slider.value.toFixed(decimals)) + unit
            color: "#ccc"
            font.pixelSize: 11
            Layout.preferredWidth: 56
            horizontalAlignment: Text.AlignRight
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScootUI 1.0

ApplicationWindow {
    id: simWindow
    title: "ScootUI Simulator"
    width: 520
    height: 900
    visible: true
    color: "#1e1e1e"

    x: Screen.width / 2 - (480 + width + 20) / 2 + 480 + 20
    y: Math.max(0, Screen.height / 2 - height / 2)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Pinned header — Screenshot + screen / theme / lang pickers always
        // in reach.
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            SimButton {
                text: "📸"
                small: true
                color: "#607D8B"
                Layout.minimumWidth: 28
                onClicked: SimulatorService.takeScreenshot()
            }
            Item { Layout.preferredWidth: 4 }

            ButtonGroup { id: screenGroup; exclusive: true }
            SimButton {
                text: "Cluster"; small: true
                ButtonGroup.group: screenGroup
                checkable: true; checked: true
                onClicked: ScreenStore.setScreen(0)
            }
            SimButton {
                text: "Map"; small: true
                ButtonGroup.group: screenGroup
                checkable: true
                onClicked: ScreenStore.setScreen(1)
            }
            SimButton {
                text: "About"; small: true
                ButtonGroup.group: screenGroup
                checkable: true
                onClicked: ScreenStore.setScreen(4)
            }
            SimButton {
                text: "SysInfo"; small: true
                ButtonGroup.group: screenGroup
                checkable: true
                // Re-tapping cycles System / Connectivity / Batteries, which on
                // the vehicle are three separate System > Info menu entries.
                onClicked: ScreenStore.showSystemInfo(
                    ScreenStore.currentScreen === 15 // ScreenMode.SystemInfo
                        ? (ScreenStore.systemInfoPage + 1) % 3 : 0)
            }
            SimButton {
                text: "Debug"; small: true
                ButtonGroup.group: screenGroup
                checkable: true
                onClicked: ScreenStore.setScreen(3) // ScreenMode.Debug
            }

            Item { Layout.fillWidth: true }

            ButtonGroup { id: themeGroup; exclusive: true }
            SimButton {
                text: "Dark"; small: true
                ButtonGroup.group: themeGroup
                checkable: true; checked: true
                onClicked: SimulatorService.setTheme("dark")
            }
            SimButton {
                text: "Light"; small: true
                ButtonGroup.group: themeGroup
                checkable: true
                onClicked: SimulatorService.setTheme("light")
            }
            SimButton {
                text: "Auto"; small: true
                ButtonGroup.group: themeGroup
                checkable: true
                onClicked: SimulatorService.setTheme("auto")
            }

            Item { Layout.preferredWidth: 4 }

            SimButton {
                text: "BL Auto"
                onClicked: SimulatorService.setSetting("dashboard.backlight-mode", "auto")
            }
            SimButton {
                text: "BL Low"
                onClicked: SimulatorService.setSetting("dashboard.backlight-mode", "low")
            }
            SimButton {
                text: "BL Med"
                onClicked: SimulatorService.setSetting("dashboard.backlight-mode", "medium")
            }
            SimButton {
                text: "BL High"
                onClicked: SimulatorService.setSetting("dashboard.backlight-mode", "high")
            }

            Item { Layout.preferredWidth: 4 }

            ButtonGroup { id: langGroup; exclusive: true }
            SimButton {
                text: "EN"; small: true
                ButtonGroup.group: langGroup
                checkable: true; checked: true
                onClicked: SimulatorService.setLanguage("en")
            }
            SimButton {
                text: "DE"; small: true
                ButtonGroup.group: langGroup
                checkable: true
                onClicked: SimulatorService.setLanguage("de")
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#444" }

    ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: rootLayout
            width: scroll.availableWidth
            spacing: 4

            SectionHeader { text: "Auto-Drive" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimButton {
                    text: SimulatorService.autoDriveActive ? "Stop" : "Start"
                    color: SimulatorService.autoDriveActive ? "#f44336" : "#4caf50"
                    Layout.preferredWidth: 80
                    Layout.fillWidth: false
                    onClicked: {
                        if (SimulatorService.autoDriveActive)
                            SimulatorService.stopAutoDrive()
                        else
                            SimulatorService.startAutoDrive(autoDriveSpeedSlider.value)
                    }
                }
                Slider {
                    id: autoDriveSpeedSlider
                    Layout.fillWidth: true
                    from: 5; to: 55; value: 25; stepSize: 1
                    onMoved: {
                        if (SimulatorService.autoDriveActive)
                            SimulatorService.startAutoDrive(value)
                    }
                }
                Text {
                    text: Math.round(autoDriveSpeedSlider.value) + " km/h"
                    color: SimulatorService.autoDriveActive ? "#4caf50" : "#ccc"
                    font.pixelSize: 11
                    Layout.preferredWidth: 56
                    horizontalAlignment: Text.AlignRight
                }
            }
            Text {
                Layout.fillWidth: true
                visible: SimulatorService.autoDriveActive
                text: "Driving at " + SimulatorService.autoDriveSpeed.toFixed(1) + " km/h"
                color: "#4caf50"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
            }

            SectionHeader { text: "Nav availability" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Maps" }
                Switch {
                    id: mapsAvailSwitch
                    checked: typeof NavigationAvailabilityService !== "undefined"
                             && NavigationAvailabilityService.localDisplayMapsAvailable
                    palette.highlight: "#2196F3"
                    onToggled: {
                        if (typeof NavigationAvailabilityService !== "undefined")
                            NavigationAvailabilityService.setOverride(checked, routingAvailSwitch.checked)
                    }
                }
                SimLabel { text: "Routing" }
                Switch {
                    id: routingAvailSwitch
                    checked: typeof NavigationAvailabilityService !== "undefined"
                             && NavigationAvailabilityService.routingAvailable
                    palette.highlight: "#2196F3"
                    onToggled: {
                        if (typeof NavigationAvailabilityService !== "undefined")
                            NavigationAvailabilityService.setOverride(mapsAvailSwitch.checked, checked)
                    }
                }
                Item { Layout.fillWidth: true }
                SimButton {
                    text: "Auto"; small: true; color: "#666"
                    onClicked: {
                        if (typeof NavigationAvailabilityService !== "undefined")
                            NavigationAvailabilityService.clearOverride()
                    }
                }
            }

            SectionHeader {
                text: "Routes"
                SimButton {
                    text: "Clear"; small: true; color: "#f44336"
                    onClicked: {
                        if (typeof NavigationService !== "undefined")
                            NavigationService.clearNavigation()
                    }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 4
                rowSpacing: 4
                SimButton { text: "C-burg → Moabit"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadTestRoute(1) }
                SimButton { text: "Mitte → Moabit"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadTestRoute(2) }
                SimButton { text: "Tempelhof → F'hain"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadTestRoute(3) }
                SimButton { text: "Short"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadTestRoute(4) }
                SimButton { text: "Roundabout"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadTestRoute(5) }
                SimButton { text: "U-turn start"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadTestRoute(6) }
            }

            SectionHeader { text: "Vehicle Presets" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 3
                SimButton { text: "Parked"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("parked") }
                SimButton { text: "Ready"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("ready") }
                SimButton { text: "Driving"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("driving") }
                SimButton { text: "Fast"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("driving-fast") }
                SimButton { text: "LowBatt"; small: true; color: "#ff6b35"; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("low-battery") }
                SimButton { text: "Updating"; small: true; color: "#9c27b0"; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("updating") }
                SimButton { text: "NoGPS"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("no-gps") }
                SimButton { text: "Offline"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("offline") }
                SimButton { text: "1Batt"; small: true; Layout.fillWidth: true; onClicked: SimulatorService.loadPreset("single-battery") }
            }

            SectionHeader { text: "Overrides" }

            // Trip block — edit any two of {Duration, Avg, Trip distance};
            // the third recomputes. Most-recently-edited two are kept; the
            // stale one is overwritten.
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                property var lastTwo: ["dur", "avg"]

                function noteEdit(which) {
                    if (lastTwo[0] === which) return recompute()
                    lastTwo = [which, lastTwo[0]]
                    recompute()
                }

                function recompute() {
                    var dist = parseFloat(tripDistField.text); if (isNaN(dist)) dist = 0
                    var dur  = parseFloat(tripDurField.text);  if (isNaN(dur))  dur  = 0
                    var avg  = parseFloat(tripAvgField.text);  if (isNaN(avg))  avg  = 0
                    var stale = ["dur", "avg", "dist"].find(function(f){ return lastTwo.indexOf(f) < 0 })
                    if (stale === "dist") {
                        dist = avg * (dur / 3600.0)
                        tripDistField.text = dist.toFixed(2)
                    } else if (stale === "dur" && avg > 0) {
                        dur = (dist / avg) * 3600.0
                        tripDurField.text = Math.round(dur).toString()
                    } else if (stale === "avg" && dur > 0) {
                        avg = dist / (dur / 3600.0)
                        tripAvgField.text = avg.toFixed(1)
                    }
                    if (typeof TripStore !== "undefined")
                        TripStore.setOverride(dist, Math.round(dur), avg)
                }

                Text { text: "clock"; color: "#999"; font.pixelSize: 9 }
                TextField {
                    id: clockOverrideField
                    Layout.preferredWidth: 56
                    placeholderText: "HH:mm"
                    text: SimulatorService.clockOverride
                    color: "white"; font.pixelSize: 10
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: SimulatorService.clockOverride = text
                }
                Item { Layout.preferredWidth: 6 }
                Text { text: "date"; color: "#999"; font.pixelSize: 9 }
                TextField {
                    id: dateOverrideField
                    Layout.preferredWidth: 76
                    placeholderText: "yyyy-MM-dd"
                    text: SimulatorService.dateOverride
                    color: "white"; font.pixelSize: 10
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: SimulatorService.dateOverride = text
                }
                Item { Layout.preferredWidth: 6 }
                Text { text: "dur s"; color: "#999"; font.pixelSize: 9 }
                TextField {
                    id: tripDurField
                    Layout.preferredWidth: 56
                    text: "1830"
                    color: "white"; font.pixelSize: 10
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: parent.noteEdit("dur")
                }
                Text { text: "avg"; color: "#999"; font.pixelSize: 9 }
                TextField {
                    id: tripAvgField
                    Layout.preferredWidth: 48
                    text: "24.2"
                    color: "white"; font.pixelSize: 10
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: parent.noteEdit("avg")
                }
                Text { text: "trip"; color: "#999"; font.pixelSize: 9 }
                TextField {
                    id: tripDistField
                    Layout.preferredWidth: 44
                    text: "12.3"
                    color: "white"; font.pixelSize: 10
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: parent.noteEdit("dist")
                }
                Text { text: "total"; color: "#999"; font.pixelSize: 9 }
                TextField {
                    id: odometerOverrideField
                    Layout.preferredWidth: 52
                    text: "0.0"
                    color: "white"; font.pixelSize: 10
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: {
                        var v = parseFloat(text)
                        if (!isNaN(v)) SimulatorService.setOdometer(v)
                    }
                    Component.onCompleted: SimulatorService.setOdometer(0)
                }
                Switch {
                    id: freezeTripSwitch
                    checked: false
                    scale: 0.6
                    Layout.preferredWidth: 32
                    palette.highlight: "#2196F3"
                    ToolTip.text: "Freeze trip timer"
                    ToolTip.visible: hovered
                    onToggled: {
                        if (typeof TripStore === "undefined") return
                        if (checked) {
                            TripStore.setOverride(TripStore.distance,
                                                  TripStore.duration,
                                                  TripStore.averageSpeed)
                        } else {
                            TripStore.clearOverride()
                        }
                    }
                }
                SimButton {
                    text: "Clear"; small: true; color: "#666"
                    onClicked: {
                        clockOverrideField.text = ""
                        SimulatorService.clockOverride = ""
                        dateOverrideField.text = ""
                        SimulatorService.dateOverride = ""
                        if (typeof TripStore !== "undefined")
                            TripStore.clearOverride()
                        freezeTripSwitch.checked = false
                    }
                }
            }

            SectionHeader { text: "Engine" }
            // Speed slider only writes the cluster needle; auto-drive remains
            // the only way to advance position along a route.
            SimSliderRow {
                label: "Speed"
                from: 0; to: 100; value: 0; unit: "km/h"; decimals: 0
                onMoved: function(v) { SimulatorService.setSpeed(v) }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 3
                Text { text: "main"; color: "#999"; font.pixelSize: 9 }
                SimButton { text: "On"; small: true; onClicked: SimulatorService.setMainPower(true) }
                SimButton { text: "Off"; small: true; color: "#f44336"; onClicked: SimulatorService.setMainPower(false) }
                Text { text: "motor"; color: "#999"; font.pixelSize: 9 }
                SimButton { text: "On"; small: true; onClicked: SimulatorService.setMotorPower(true) }
                SimButton { text: "Off"; small: true; color: "#f44336"; onClicked: SimulatorService.setMotorPower(false) }
                Text { text: "KERS"; color: "#999"; font.pixelSize: 9 }
                SimButton { text: "On"; small: true; onClicked: SimulatorService.setKers(true) }
                SimButton { text: "Off"; small: true; color: "#f44336"; onClicked: SimulatorService.setKers(false) }
                Text { text: "thr"; color: "#999"; font.pixelSize: 9 }
                SimButton { text: "On"; small: true; onClicked: SimulatorService.setThrottle(true) }
                SimButton { text: "Off"; small: true; color: "#f44336"; onClicked: SimulatorService.setThrottle(false) }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                spacing: 4
                Text { text: "regen"; color: "#999"; font.pixelSize: 9 }
                SimButton { text: "OK"; small: true; onClicked: SimulatorService.setRegenReason("none") }
                SimButton { text: "cold"; small: true; onClicked: SimulatorService.setRegenReason("cold") }
                SimButton { text: "hot"; small: true; onClicked: SimulatorService.setRegenReason("hot") }
                SimButton { text: "full"; small: true; onClicked: SimulatorService.setRegenReason("full") }
                SimButton { text: "off"; small: true; color: "#f44336"; onClicked: SimulatorService.setRegenReason("off") }
                Item { Layout.fillWidth: true }
            }

            CollapsibleSection {
                title: "Engine (extras)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SimSliderRow {
                        label: "Eng T"
                        from: -10; to: 120; value: 25; unit: "°C"; decimals: 0
                        onMoved: function(v) { SimulatorService.setEngineTemperature(v) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimSliderRow {
                            Layout.fillWidth: true
                            label: "Ambient"
                            from: -20; to: 50; value: 18.5; unit: "°C"; decimals: 1
                            onMoved: function(v) { SimulatorService.setAmbientTemperature(v) }
                        }
                        SimButton {
                            text: "Clear"; small: true
                            onClicked: SimulatorService.clearAmbientTemperature()
                        }
                    }
                    SimSliderRow {
                        label: "Motor I"
                        from: -10000; to: 80000; value: 0; unit: "mA"; decimals: 0
                        onMoved: function(v) { SimulatorService.setMotorCurrent(v) }
                    }
                    SimSliderRow {
                        label: "Motor V"
                        from: 0; to: 60000; value: 54000; unit: "mV"; decimals: 0
                        onMoved: function(v) { SimulatorService.setMotorVoltage(v) }
                    }
                    SimSliderRow {
                        label: "RPM"
                        from: 0; to: 8000; value: 0; unit: ""; decimals: 0
                        onMoved: function(v) { SimulatorService.setRpm(v) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "Easter" }
                        CheckBox {
                            checked: typeof OdometerMilestoneService !== "undefined"
                                     && OdometerMilestoneService.easterEggsEnabled
                            onToggled: {
                                if (typeof OdometerMilestoneService !== "undefined")
                                    OdometerMilestoneService.easterEggsEnabled = checked
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
                            onClicked: SimulatorService.setEngineFault(parseInt(faultCodeField.text) || 0,
                                                                faultDescField.text)
                        }
                    }
                }
            }

            SectionHeader { text: "Vehicle State" }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: "state"; color: "#999"; font.pixelSize: 9 }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    font.pixelSize: 10
                    model: ["parked", "ready-to-drive", "stand-by", "off",
                            "shutting-down", "booting", "hibernating",
                            "waiting-hibernation", "updating"]
                    currentIndex: 0
                    onActivated: SimulatorService.setVehicleState(currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                    palette.highlight: "#2196F3"
                }
                Text { text: "k-stand"; color: "#999"; font.pixelSize: 9 }
                Switch {
                    id: kickstandSwitch
                    checked: true
                    scale: 0.7
                    Layout.preferredWidth: 36
                    palette.highlight: "#2196F3"
                    onToggled: SimulatorService.setKickstand(checked ? "down" : "up")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 3
                Text { text: "blink"; color: "#999"; font.pixelSize: 9 }
                ButtonGroup { id: blinkerGroup; exclusive: true }
                SimButton { text: "Off"; small: true; ButtonGroup.group: blinkerGroup; checkable: true; checked: true; onClicked: SimulatorService.setBlinkerState("off") }
                SimButton { text: "L"; small: true; ButtonGroup.group: blinkerGroup; checkable: true; onClicked: SimulatorService.setBlinkerState("left") }
                SimButton { text: "R"; small: true; ButtonGroup.group: blinkerGroup; checkable: true; onClicked: SimulatorService.setBlinkerState("right") }
                SimButton { text: "Both"; small: true; ButtonGroup.group: blinkerGroup; checkable: true; onClicked: SimulatorService.setBlinkerState("both") }
                Text { text: "brake"; color: "#999"; font.pixelSize: 9 }
                SimButton {
                    text: "L"; small: true
                    onPressed: SimulatorService.setBrakeLeft(true)
                    onReleased: SimulatorService.setBrakeLeft(false)
                }
                SimButton {
                    text: "R"; small: true
                    onPressed: SimulatorService.setBrakeRight(true)
                    onReleased: SimulatorService.setBrakeRight(false)
                }
                Text { text: "btn"; color: "#999"; font.pixelSize: 9 }
                SimButton {
                    text: "S-Box"; small: true
                    onPressed: SimulatorService.setSeatboxButton(true)
                    onReleased: SimulatorService.setSeatboxButton(false)
                }
                SimButton {
                    text: "Horn"; small: true; color: "#ff9800"
                    onPressed: SimulatorService.setHornButton(true)
                    onReleased: SimulatorService.setHornButton(false)
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                SimLabel { text: "Locks" }
                SimButton {
                    text: "S-Box Open"; small: true; onClicked: SimulatorService.setSeatboxLock("open")
                }
                SimButton {
                    text: "S-Box Close"; small: true; onClicked: SimulatorService.setSeatboxLock("closed")
                }
                SimButton {
                    text: "H-Bar Lock"; small: true; onClicked: SimulatorService.setHandlebarLock("locked")
                }
                SimButton {
                    text: "H-Bar Unlock"; small: true; onClicked: SimulatorService.setHandlebarLock("unlocked")
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
                            onClicked: SimulatorService.setHandlebarPosition("on-place") }
                        SimButton { text: "Off-place"; small: true
                            onClicked: SimulatorService.setHandlebarPosition("off-place") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Drive" }
                        SimButton { text: "Unable"; small: true; color: "#f44336"
                            onClicked: SimulatorService.setUnableToDrive(true) }
                        SimButton { text: "Able"; small: true
                            onClicked: SimulatorService.setUnableToDrive(false) }
                        Item { Layout.preferredWidth: 8 }
                        SimLabel { text: "Hop-on" }
                        SimButton { text: "Active"; small: true
                            onClicked: SimulatorService.setHopOnActive(true) }
                        SimButton { text: "Idle"; small: true
                            onClicked: SimulatorService.setHopOnActive(false) }
                    }
                }
            }

            SectionHeader { text: "GPS" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "Freeze" }
                Switch {
                    checked: SimulatorService.gpsFrozen
                    onToggled: {
                        SimulatorService.gpsFrozen = checked
                        if (typeof MapService !== "undefined")
                            MapService.deadReckoningPaused = checked
                    }
                    palette.highlight: "#2196F3"
                }
                SimLabel { text: "State" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["fix-established", "searching", "off", "error"]
                    currentIndex: 0
                    onActivated: SimulatorService.setGpsState(currentText)
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
                    onEditingFinished: SimulatorService.setGpsPosition(
                        parseFloat(text), parseFloat(lngField.text))
                }
                SimLabel { text: "Lng" }
                TextField {
                    id: lngField
                    Layout.fillWidth: true
                    text: "13.404954"
                    color: "white"; font.pixelSize: 12
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: SimulatorService.setGpsPosition(
                        parseFloat(latField.text), parseFloat(text))
                }
            }
            SimSliderRow {
                label: "Course"
                from: 0; to: 359; value: 0; unit: "°"; decimals: 0
                onMoved: function(v) { SimulatorService.setGpsCourse(v) }
            }

            CollapsibleSection {
                title: "GPS (deep)"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SimSliderRow {
                        label: "Alt"
                        from: -50; to: 1500; value: 34; unit: "m"; decimals: 0
                        onMoved: function(v) { SimulatorService.setGpsAltitude(v) }
                    }
                    SimSliderRow {
                        label: "Hdop"
                        from: 0; to: 25; value: 1.0; unit: ""; decimals: 1
                        onMoved: function(v) { SimulatorService.setGpsHdop(v) }
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
                            onClicked: SimulatorService.setGpsSatellites(parseInt(satsUsedField.text) || 0,
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
                            onClicked: SimulatorService.setGpsField(gpsFieldName.text, gpsFieldValue.text)
                        }
                    }
                }
            }

            SectionHeader { text: "Battery" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                SimLabel { text: "B0" }
                Switch {
                    checked: true
                    palette.highlight: "#2196F3"
                    onToggled: SimulatorService.setBatteryPresent(0, checked)
                }
                ComboBox {
                    Layout.preferredWidth: 84
                    model: ["active", "idle", "asleep", "unknown"]
                    currentIndex: 0
                    onActivated: SimulatorService.setBatteryState(0, currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                Slider {
                    id: b0Slider
                    Layout.fillWidth: true
                    from: 0; to: 100; value: 80; stepSize: 1
                    onMoved: SimulatorService.setBatteryCharge(0, Math.round(value))
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
                    onToggled: SimulatorService.setBatteryPresent(1, checked)
                }
                ComboBox {
                    Layout.preferredWidth: 84
                    model: ["active", "idle", "asleep", "unknown"]
                    currentIndex: 1
                    onActivated: SimulatorService.setBatteryState(1, currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                Slider {
                    id: b1Slider
                    Layout.fillWidth: true
                    from: 0; to: 100; value: 75; stepSize: 1
                    onMoved: SimulatorService.setBatteryCharge(1, Math.round(value))
                }
                Text {
                    text: Math.round(b1Slider.value) + "%"
                    color: "#ccc"; font.pixelSize: 11
                    Layout.preferredWidth: 36
                    horizontalAlignment: Text.AlignRight
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
                                    onEditingFinished: SimulatorService.setBatteryVoltage(slotCol.slot,
                                                                                   parseInt(text) || 0)
                                }
                                SimLabel { text: "I (mA)" }
                                TextField {
                                    Layout.preferredWidth: 80
                                    text: "0"
                                    color: "white"; font.pixelSize: 11
                                    background: Rectangle { color: "#333"; radius: 3 }
                                    onEditingFinished: SimulatorService.setBatteryCurrent(slotCol.slot,
                                                                                   parseInt(text) || 0)
                                }
                                SimSliderRow {
                                    Layout.fillWidth: true
                                    label: "T"
                                    from: -20; to: 60; value: 25; unit: "°C"; decimals: 0
                                    onMoved: function(v) {
                                        SimulatorService.setBatteryTemperature(slotCol.slot, Math.round(v))
                                    }
                                }
                            }
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
                            onToggled: SimulatorService.setCbBatteryField("present", checked ? "true" : "false")
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
                            onEditingFinished: SimulatorService.setCbBatteryField("charge", text)
                        }
                        SimLabel { text: "Temp" }
                        TextField {
                            Layout.preferredWidth: 64
                            text: "23"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: SimulatorService.setCbBatteryField("temperature", text)
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
                            onEditingFinished: SimulatorService.setAuxBatteryField("voltage", text)
                        }
                        SimLabel { text: "Charge" }
                        TextField {
                            Layout.preferredWidth: 64
                            text: "100"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: SimulatorService.setAuxBatteryField("charge", text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Status" }
                        SimButton { text: "not-charging"; small: true
                            onClicked: SimulatorService.setAuxBatteryField("charge-status", "not-charging") }
                        SimButton { text: "bulk"; small: true
                            onClicked: SimulatorService.setAuxBatteryField("charge-status", "bulk-charge") }
                        SimButton { text: "absorb"; small: true
                            onClicked: SimulatorService.setAuxBatteryField("charge-status", "absorption-charge") }
                        SimButton { text: "float"; small: true
                            onClicked: SimulatorService.setAuxBatteryField("charge-status", "float-charge") }
                    }
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
                    onActivated: SimulatorService.setOtaStatus("mdb", currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                SimLabel { text: "DBC" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["idle", "downloading", "preparing", "installing",
                            "pending-reboot", "error"]
                    currentIndex: 0
                    onActivated: SimulatorService.setOtaStatus("dbc", currentText)
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
                    onClicked: SimulatorService.setOtaUpdateMethod("mdb", "delta")
                }
                SimButton {
                    text: "MDB full"; small: true
                    ButtonGroup.group: otaMethodMdbGroup
                    checkable: true
                    onClicked: SimulatorService.setOtaUpdateMethod("mdb", "full")
                }
                Item { Layout.preferredWidth: 8 }
                ButtonGroup { id: otaMethodDbcGroup; exclusive: true }
                SimButton {
                    text: "DBC delta"; small: true
                    ButtonGroup.group: otaMethodDbcGroup
                    checkable: true
                    onClicked: SimulatorService.setOtaUpdateMethod("dbc", "delta")
                }
                SimButton {
                    text: "DBC full"; small: true
                    ButtonGroup.group: otaMethodDbcGroup
                    checkable: true
                    onClicked: SimulatorService.setOtaUpdateMethod("dbc", "full")
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
                        SimulatorService.setOtaDownloadProgress("mdb", v)
                        SimulatorService.setOtaDownloadProgress("dbc", v)
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
                        SimulatorService.setOtaInstallProgress("mdb", v)
                        SimulatorService.setOtaInstallProgress("dbc", v)
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
                        SimulatorService.setOtaUpdateVersion("mdb", "v0.99.0")
                        SimulatorService.setOtaUpdateVersion("dbc", "v0.99.0")
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
                        SimulatorService.setOtaError("mdb", otaErrorCombo.currentText)
                        SimulatorService.setOtaErrorMessage("mdb", "Simulated " + otaErrorCombo.currentText)
                    }
                }
                SimButton {
                    text: "Reset"; small: true; color: "#666"
                    onClicked: SimulatorService.clearOta()
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
                    onActivated: SimulatorService.setModemState(currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
                SimLabel { text: "Cloud" }
                SimButton {
                    text: "On"; small: true; onClicked: SimulatorService.setCloudConnection("connected")
                }
                SimButton {
                    text: "Off"; small: true; color: "#f44336"
                    onClicked: SimulatorService.setCloudConnection("disconnected")
                }
            }
            SimSliderRow {
                label: "Signal"
                from: 0; to: 100; value: 75; unit: "%"; decimals: 0
                onMoved: function(v) { SimulatorService.setSignalQuality(Math.round(v)) }
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
                        onClicked: SimulatorService.setAccessTech(modelData)
                    }
                }
                Item { Layout.preferredWidth: 8 }
                SimLabel { text: "BT" }
                SimButton {
                    text: "On"; small: true
                    onClicked: SimulatorService.setBluetoothStatus("connected")
                }
                SimButton {
                    text: "Off"; small: true; color: "#f44336"
                    onClicked: SimulatorService.setBluetoothStatus("disconnected")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                SimLabel { text: "USB" }
                SimButton {
                    text: "Disconnect"; small: true; color: "#f44336"
                    onClicked: ConnectionStore.simulateUsbDisconnect(true)
                }
                SimButton {
                    text: "Reconnect"; small: true; color: "#4caf50"
                    onClicked: ConnectionStore.simulateUsbDisconnect(false)
                }
                Item { Layout.preferredWidth: 8 }
                SimLabel { text: "UMS" }
                SimButton {
                    text: "Activate"; small: true; color: "#2196F3"
                    onClicked: { SimulatorService.setUsbStatus("active"); SimulatorService.setUsbMode("ums") }
                }
                SimButton {
                    text: "Exit"; small: true; color: "#f44336"
                    onClicked: { SimulatorService.setUsbStatus("idle"); SimulatorService.setUsbMode("normal") }
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
                            onEditingFinished: SimulatorService.setBluetoothMac(text)
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
                            onEditingFinished: SimulatorService.setBluetoothPin(text)
                        }
                        SimButton {
                            text: "123456"; small: true
                            onClicked: { btPinField.text = "123456"; SimulatorService.setBluetoothPin("123456") }
                        }
                        SimButton {
                            text: "Clear"; small: true; color: "#666"
                            onClicked: { btPinField.text = ""; SimulatorService.setBluetoothPin("") }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Health" }
                        SimButton { text: "ok"; small: true; onClicked: SimulatorService.setBluetoothServiceHealth("ok") }
                        SimButton { text: "warn"; small: true; onClicked: SimulatorService.setBluetoothServiceHealth("warn") }
                        SimButton { text: "error"; small: true; color: "#f44336"
                            onClicked: SimulatorService.setBluetoothServiceHealth("error") }
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
                            onEditingFinished: SimulatorService.setBluetoothServiceError(text)
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
                            onEditingFinished: SimulatorService.setIpAddress(text)
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
                            onEditingFinished: SimulatorService.setSimImei(text)
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
                            onEditingFinished: SimulatorService.setSimImsi(text)
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
                            onEditingFinished: SimulatorService.setSimIccid(text)
                        }
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
                            onEditingFinished: SimulatorService.setUsbStep(text)
                        }
                    }
                    SimSliderRow {
                        label: "Progress"
                        from: 0; to: 100; value: 0; unit: "%"; decimals: 0
                        onMoved: function(v) { SimulatorService.setUsbProgress(Math.round(v)) }
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
                            onEditingFinished: SimulatorService.setUsbDetail(text)
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
                            onClicked: { SimulatorService.pushUmsLog(umsLogField.text); umsLogField.text = "" }
                        }
                    }
                }
            }

            CollapsibleSection {
                title: "Auto-Lock"
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Trigger" }
                        SimButton { text: "60s"; small: true; onClicked: SimulatorService.setAutoStandbyDeadline(60) }
                        SimButton { text: "30s"; small: true; onClicked: SimulatorService.setAutoStandbyDeadline(30) }
                        SimButton { text: "10s"; small: true; onClicked: SimulatorService.setAutoStandbyDeadline(10) }
                        SimButton { text: "Clear"; small: true; color: "#f44336"
                            onClicked: SimulatorService.clearAutoStandbyDeadline() }
                    }
                    SimSliderRow {
                        label: "Timeout"
                        from: 0; to: 1800; value: 900; unit: "s"; decimals: 0
                        onMoved: function(v) { SimulatorService.setAutoStandbySetting(Math.round(v)) }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: typeof AutoStandbyStore !== "undefined"
                                 && AutoStandbyStore.remainingSeconds > 0
                        text: "Remaining: " + (typeof AutoStandbyStore !== "undefined"
                              ? AutoStandbyStore.remainingSeconds : 0) + "s"
                        color: typeof AutoStandbyStore !== "undefined"
                               && AutoStandbyStore.remainingSeconds <= 60 ? "#FF9800" : "#4caf50"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
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
                            onEditingFinished: SimulatorService.setSpeedLimit(text)
                        }
                        SimLabel { text: "Type" }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["", "residential", "secondary", "primary", "motorway",
                                    "tertiary", "service", "footway"]
                            onActivated: SimulatorService.setRoadType(currentText)
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
                            onEditingFinished: SimulatorService.setRoadName(text)
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
                            onEditingFinished: SimulatorService.setRoadRefs(text)
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
                                    onClicked: SimulatorService.setSetting(visRow.settingKey, modelData)
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Blinker" }
                        SimButton { text: "Icon"; small: true
                            onClicked: SimulatorService.setSetting("dashboard.blinker-style", "icon") }
                        SimButton { text: "Overlay"; small: true
                            onClicked: SimulatorService.setSetting("dashboard.blinker-style", "overlay") }
                        Item { Layout.preferredWidth: 8 }
                        SimLabel { text: "DBC LED" }
                        SimButton { text: "On"; small: true
                            onClicked: SimulatorService.setSetting("scooter.dbc-blinker-led", "enabled") }
                        SimButton { text: "Off"; small: true
                            onClicked: SimulatorService.setSetting("scooter.dbc-blinker-led", "disabled") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Alarm" }
                        SimButton { text: "Enable"; small: true; color: "#4caf50"
                            onClicked: SimulatorService.setSetting("alarm.enabled", "true") }
                        SimButton { text: "Disable"; small: true; color: "#f44336"
                            onClicked: SimulatorService.setSetting("alarm.enabled", "false") }
                        SimButton { text: "Honk"; small: true
                            onClicked: SimulatorService.setSetting("alarm.honk", "true") }
                        SimButton { text: "10s"; small: true
                            onClicked: SimulatorService.setSetting("alarm.duration", "10") }
                        SimButton { text: "30s"; small: true
                            onClicked: SimulatorService.setSetting("alarm.duration", "30") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Battery" }
                        SimButton { text: "Single"; small: true
                            onClicked: SimulatorService.setDualBattery(false) }
                        SimButton { text: "Dual"; small: true
                            onClicked: SimulatorService.setDualBattery(true) }
                        Item { Layout.preferredWidth: 8 }
                        SimLabel { text: "Map" }
                        SimButton { text: "Online"; small: true
                            onClicked: SimulatorService.setSetting("dashboard.map.type", "online") }
                        SimButton { text: "Offline"; small: true
                            onClicked: SimulatorService.setSetting("dashboard.map.type", "offline") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SimLabel { text: "Traffic" }
                        Switch {
                            checked: false
                            palette.highlight: "#2196F3"
                            onToggled: SimulatorService.setTrafficOverlay(checked)
                        }
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
                            onClicked: SimulatorService.setDebugOverlay("off") }
                        SimButton { text: "Overlay"; small: true
                            onClicked: SimulatorService.setDebugOverlay("overlay") }
                    }
                    SimSliderRow {
                        label: "Bright"
                        from: 0; to: 1500; value: 200; unit: "lx"; decimals: 0
                        onMoved: function(v) { SimulatorService.setBrightness(v) }
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
                            text: "stable-v1.0.0"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: SimulatorService.setMdbVersion(text)
                            Component.onCompleted: SimulatorService.setMdbVersion(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "DBC" }
                        TextField {
                            Layout.fillWidth: true
                            text: "stable-v1.0.0"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: SimulatorService.setDbcVersion(text)
                            Component.onCompleted: SimulatorService.setDbcVersion(text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SimLabel { text: "nRF" }
                        TextField {
                            Layout.fillWidth: true
                            text: "v2.3.0-ls"
                            color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#333"; radius: 3 }
                            onEditingFinished: SimulatorService.setSystemField("nrf-fw-version", text)
                            Component.onCompleted: SimulatorService.setSystemField("nrf-fw-version", text)
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
                            onClicked: SimulatorService.setRaw(rawChannel.text, rawField.text, rawValue.text)
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
                            onClicked: SimulatorService.publishMessage(pubChannel.text, pubMessage.text)
                        }
                        SimButton {
                            text: "LPush"; small: true
                            onClicked: SimulatorService.pushList(pubChannel.text, pubMessage.text)
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
    }

    component SectionHeader: RowLayout {
        property alias text: headerText.text
        default property alias rightContent: rightHolder.children
        Layout.fillWidth: true
        Layout.topMargin: 8
        Layout.bottomMargin: 0
        spacing: 6
        Text {
            id: headerText
            color: "#2196F3"
            font.pixelSize: 9
            font.bold: true
            font.letterSpacing: 0.5
            font.capitalization: Font.AllUppercase
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.alignment: Qt.AlignVCenter
            color: "#3a3a3a"
        }
        Item {
            id: rightHolder
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
            Layout.preferredHeight: 22
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
        font.pixelSize: 10
        Layout.preferredWidth: 44
    }

    component SimButton: Button {
        property bool small: false
        property color color: "#555"
        Layout.fillWidth: !small
        Layout.preferredWidth: small ? -1 : -1
        Layout.minimumWidth: small ? 32 : 56
        Layout.preferredHeight: small ? 22 : 26
        padding: small ? 4 : 6
        font.pixelSize: small ? 10 : 11
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

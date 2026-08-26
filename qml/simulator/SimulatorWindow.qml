import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScootUI 1.0

ApplicationWindow {
    id: simWindow
    // Every control here writes into whichever repository backs the app, so the
    // title says which one. Against a real Redis that is worth knowing before
    // pressing anything.
    title: "ScootUI Simulator (" + (typeof simulatorBackend !== "undefined"
                                    ? simulatorBackend : "?") + ")"
    // Kept in step with Main.qml, which positions the 480 px UI window to the
    // left of this one by the same gap. Wide enough that the pinned header row
    // fits without horizontal scrolling.
    readonly property int uiWidth: 480
    readonly property int uiGap: 64

    width: 900
    height: 900
    visible: true
    color: "#1e1e1e"

    x: Screen.width / 2 - (uiWidth + width + uiGap) / 2 + uiWidth + uiGap
    y: Math.max(0, Screen.height / 2 - height / 2)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Pinned header: screenshot, screen picker and the display settings
        // that get flipped constantly. One fixed button width per group, so a
        // group reads as a block instead of a ragged run.
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            SimButton {
                text: "📸"
                small: true
                color: "#607D8B"
                fixedWidth: 30
                onClicked: simulator.takeScreenshot()
            }
            Item { Layout.preferredWidth: 8 }

            ButtonGroup { id: screenGroup; exclusive: true }
            SimButton {
                text: "Cluster"; small: true; fixedWidth: 54
                ButtonGroup.group: screenGroup
                checkable: true; checked: true
                onClicked: screenStore.setScreen(Scooter.ScreenMode.Cluster)
            }
            SimButton {
                text: "Map"; small: true; fixedWidth: 54
                ButtonGroup.group: screenGroup
                checkable: true
                onClicked: screenStore.setScreen(Scooter.ScreenMode.Map)
            }
            SimButton {
                text: "About"; small: true; fixedWidth: 54
                ButtonGroup.group: screenGroup
                checkable: true
                onClicked: screenStore.setScreen(Scooter.ScreenMode.About)
            }
            SimButton {
                text: "SysInfo"; small: true; fixedWidth: 54
                ButtonGroup.group: screenGroup
                checkable: true
                // Re-tapping cycles System / Connectivity / Batteries, which on
                // the vehicle are three separate System > Info menu entries.
                onClicked: screenStore.showSystemInfo(
                    screenStore.currentScreen === Scooter.ScreenMode.SystemInfo
                        ? (screenStore.systemInfoPage + 1) % 3 : 0)
            }
            SimButton {
                text: "Debug"; small: true; fixedWidth: 54
                ButtonGroup.group: screenGroup
                checkable: true
                onClicked: screenStore.setScreen(Scooter.ScreenMode.Debug)
            }

            Item { Layout.fillWidth: true }

            SimButton {
                text: "BL Auto"; small: true; fixedWidth: 52
                onClicked: simulator.setSetting("dashboard.backlight-mode", "auto")
            }
            SimButton {
                text: "BL Low"; small: true; fixedWidth: 52
                onClicked: simulator.setSetting("dashboard.backlight-mode", "low")
            }
            SimButton {
                text: "BL Med"; small: true; fixedWidth: 52
                onClicked: simulator.setSetting("dashboard.backlight-mode", "medium")
            }
            SimButton {
                text: "BL High"; small: true; fixedWidth: 52
                onClicked: simulator.setSetting("dashboard.backlight-mode", "high")
            }

            Item { Layout.preferredWidth: 8 }

            ButtonGroup { id: themeGroup; exclusive: true }
            SimButton {
                text: "Dark"; small: true; fixedWidth: 44
                ButtonGroup.group: themeGroup
                checkable: true; checked: true
                onClicked: simulator.setTheme("dark")
            }
            SimButton {
                text: "Light"; small: true; fixedWidth: 44
                ButtonGroup.group: themeGroup
                checkable: true
                onClicked: simulator.setTheme("light")
            }
            SimButton {
                text: "Auto"; small: true; fixedWidth: 44
                ButtonGroup.group: themeGroup
                checkable: true
                onClicked: simulator.setTheme("auto")
            }

            Item { Layout.preferredWidth: 8 }

            ButtonGroup { id: langGroup; exclusive: true }
            SimButton {
                text: "EN"; small: true; fixedWidth: 32
                ButtonGroup.group: langGroup
                checkable: true; checked: true
                onClicked: simulator.setLanguage("en")
            }
            SimButton {
                text: "DE"; small: true; fixedWidth: 32
                ButtonGroup.group: langGroup
                checkable: true
                onClicked: simulator.setLanguage("de")
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

                SimSplit {
                    SimPane {
                        SectionHeader { text: "Auto-Drive" }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            SimButton {
                                text: simulator.autoDriveActive ? "Stop" : "Start"
                                color: simulator.autoDriveActive ? "#f44336" : "#4caf50"
                                fixedWidth: 72
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
                            SimValue {
                                text: Math.round(autoDriveSpeedSlider.value) + " km/h"
                                color: simulator.autoDriveActive ? "#4caf50" : "#ccc"
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

                        SectionHeader { text: "Vehicle Presets" }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: 4
                            rowSpacing: 4
                            // Writes a full default state. Runs automatically at
                            // startup only for the in-memory backend; against a
                            // Redis it stays deliberate, since it overwrites
                            // whatever is already in that instance.
                            SimButton { text: "Seed"; small: true; Layout.fillWidth: true
                                        color: (typeof simulatorSeeded !== "undefined" && simulatorSeeded)
                                               ? "#555555" : "#FF9800"
                                        onClicked: simulator.applyDefaults() }
                            SimButton { text: "Parked"; small: true; Layout.fillWidth: true; onClicked: simulator.loadPreset("parked") }
                            SimButton { text: "Ready"; small: true; Layout.fillWidth: true; onClicked: simulator.loadPreset("ready") }
                            SimButton { text: "Driving"; small: true; Layout.fillWidth: true; onClicked: simulator.loadPreset("driving") }
                            SimButton { text: "Fast"; small: true; Layout.fillWidth: true; onClicked: simulator.loadPreset("driving-fast") }
                            SimButton { text: "LowBatt"; small: true; color: "#ff6b35"; Layout.fillWidth: true; onClicked: simulator.loadPreset("low-battery") }
                            SimButton { text: "Updating"; small: true; color: "#9c27b0"; Layout.fillWidth: true; onClicked: simulator.loadPreset("updating") }
                            SimButton { text: "NoGPS"; small: true; Layout.fillWidth: true; onClicked: simulator.loadPreset("no-gps") }
                            SimButton { text: "Offline"; small: true; Layout.fillWidth: true; onClicked: simulator.loadPreset("offline") }
                            SimButton { text: "1Batt"; small: true; Layout.fillWidth: true; onClicked: simulator.loadPreset("single-battery") }
                        }

                        SectionHeader { text: "Engine" }
                        // Speed slider only writes the cluster needle; auto-drive
                        // remains the only way to advance position along a route.
                        SimSliderRow {
                            label: "Speed"
                            from: 0; to: 100; value: 0; unit: "km/h"; decimals: 0
                            onMoved: function(v) { simulator.setSpeed(v) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "main" }
                            SimButton { text: "On"; small: true; fixedWidth: 40; onClicked: simulator.setMainPower(true) }
                            SimButton { text: "Off"; small: true; fixedWidth: 40; color: "#f44336"; onClicked: simulator.setMainPower(false) }
                            Item { Layout.preferredWidth: 8 }
                            SimSubLabel { text: "motor" }
                            SimButton { text: "On"; small: true; fixedWidth: 40; onClicked: simulator.setMotorPower(true) }
                            SimButton { text: "Off"; small: true; fixedWidth: 40; color: "#f44336"; onClicked: simulator.setMotorPower(false) }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "KERS" }
                            SimButton { text: "On"; small: true; fixedWidth: 40; onClicked: simulator.setKers(true) }
                            SimButton { text: "Off"; small: true; fixedWidth: 40; color: "#f44336"; onClicked: simulator.setKers(false) }
                            Item { Layout.preferredWidth: 8 }
                            SimSubLabel { text: "thr" }
                            SimButton { text: "On"; small: true; fixedWidth: 40; onClicked: simulator.setThrottle(true) }
                            SimButton { text: "Off"; small: true; fixedWidth: 40; color: "#f44336"; onClicked: simulator.setThrottle(false) }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "regen" }
                            SimButton { text: "OK"; small: true; fixedWidth: 44; onClicked: simulator.setRegenReason("none") }
                            SimButton { text: "cold"; small: true; fixedWidth: 44; onClicked: simulator.setRegenReason("cold") }
                            SimButton { text: "hot"; small: true; fixedWidth: 44; onClicked: simulator.setRegenReason("hot") }
                            SimButton { text: "full"; small: true; fixedWidth: 44; onClicked: simulator.setRegenReason("full") }
                            SimButton { text: "off"; small: true; fixedWidth: 44; color: "#f44336"; onClicked: simulator.setRegenReason("off") }
                            Item { Layout.fillWidth: true }
                        }

                        CollapsibleSection {
                            title: "Engine (extras)"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
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
                                        text: "Clear"; small: true; fixedWidth: 44
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
                                    spacing: 4
                                    SimLabel { text: "Fault" }
                                    SimField {
                                        id: faultCodeField
                                        Layout.preferredWidth: 56
                                        text: "0"; placeholderText: "code"
                                    }
                                    SimField {
                                        id: faultDescField
                                        Layout.fillWidth: true
                                        placeholderText: "description"
                                    }
                                    SimButton {
                                        text: "Set"; small: true; fixedWidth: 44
                                        onClicked: simulator.setEngineFault(parseInt(faultCodeField.text) || 0,
                                                                            faultDescField.text)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
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
                            }
                        }

                        SectionHeader { text: "Vehicle State" }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "state" }
                            SimCombo {
                                Layout.fillWidth: true
                                model: ["parked", "ready-to-drive", "stand-by", "off",
                                        "shutting-down", "booting", "hibernating",
                                        "waiting-hibernation", "updating"]
                                currentIndex: 0
                                onActivated: simulator.setVehicleState(currentText)
                            }
                            SimSubLabel { text: "k-stand" }
                            SimSwitch {
                                id: kickstandSwitch
                                checked: true
                                onToggled: simulator.setKickstand(checked ? "down" : "up")
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "blink" }
                            ButtonGroup { id: blinkerGroup; exclusive: true }
                            SimButton { text: "Off"; small: true; fixedWidth: 40; ButtonGroup.group: blinkerGroup; checkable: true; checked: true; onClicked: simulator.setBlinkerState("off") }
                            SimButton { text: "L"; small: true; fixedWidth: 40; ButtonGroup.group: blinkerGroup; checkable: true; onClicked: simulator.setBlinkerState("left") }
                            SimButton { text: "R"; small: true; fixedWidth: 40; ButtonGroup.group: blinkerGroup; checkable: true; onClicked: simulator.setBlinkerState("right") }
                            SimButton { text: "Both"; small: true; fixedWidth: 40; ButtonGroup.group: blinkerGroup; checkable: true; onClicked: simulator.setBlinkerState("both") }
                            Item { Layout.preferredWidth: 8 }
                            SimSubLabel { text: "brake" }
                            SimButton {
                                text: "L"; small: true; fixedWidth: 40
                                onPressed: simulator.setBrakeLeft(true)
                                onReleased: simulator.setBrakeLeft(false)
                            }
                            SimButton {
                                text: "R"; small: true; fixedWidth: 40
                                onPressed: simulator.setBrakeRight(true)
                                onReleased: simulator.setBrakeRight(false)
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "btn" }
                            SimButton {
                                text: "S-Box"; small: true; fixedWidth: 56
                                onPressed: simulator.setSeatboxButton(true)
                                onReleased: simulator.setSeatboxButton(false)
                            }
                            SimButton {
                                text: "Horn"; small: true; fixedWidth: 56; color: "#ff9800"
                                onPressed: simulator.setHornButton(true)
                                onReleased: simulator.setHornButton(false)
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Locks" }
                            SimButton { text: "S-Box Open"; small: true; fixedWidth: 78
                                onClicked: simulator.setSeatboxLock("open") }
                            SimButton { text: "S-Box Close"; small: true; fixedWidth: 78
                                onClicked: simulator.setSeatboxLock("closed") }
                            SimButton { text: "H-Bar Lock"; small: true; fixedWidth: 78
                                onClicked: simulator.setHandlebarLock("locked") }
                            SimButton { text: "H-Bar Unlock"; small: true; fixedWidth: 78
                                onClicked: simulator.setHandlebarLock("unlocked") }
                            Item { Layout.fillWidth: true }
                        }

                        CollapsibleSection {
                            title: "Vehicle (deep)"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "H-Bar pos" }
                                    SimButton { text: "On-place"; small: true; fixedWidth: 68
                                        onClicked: simulator.setHandlebarPosition("on-place") }
                                    SimButton { text: "Off-place"; small: true; fixedWidth: 68
                                        onClicked: simulator.setHandlebarPosition("off-place") }
                                    Item { Layout.fillWidth: true }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Drive" }
                                    SimButton { text: "Unable"; small: true; fixedWidth: 56; color: "#f44336"
                                        onClicked: simulator.setUnableToDrive(true) }
                                    SimButton { text: "Able"; small: true; fixedWidth: 56
                                        onClicked: simulator.setUnableToDrive(false) }
                                    Item { Layout.preferredWidth: 8 }
                                    SimSubLabel { text: "Hop-on"; labelWidth: 44 }
                                    SimButton { text: "Active"; small: true; fixedWidth: 56
                                        onClicked: simulator.setHopOnActive(true) }
                                    SimButton { text: "Idle"; small: true; fixedWidth: 56
                                        onClicked: simulator.setHopOnActive(false) }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }

                        SectionHeader { text: "Battery" }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "B0" }
                            SimSwitch {
                                checked: true
                                onToggled: simulator.setBatteryPresent(0, checked)
                            }
                            SimCombo {
                                Layout.preferredWidth: 84
                                model: ["active", "idle", "asleep", "unknown"]
                                currentIndex: 0
                                onActivated: simulator.setBatteryState(0, currentText)
                            }
                            Slider {
                                id: b0Slider
                                Layout.fillWidth: true
                                from: 0; to: 100; value: 80; stepSize: 1
                                onMoved: simulator.setBatteryCharge(0, Math.round(value))
                            }
                            SimValue { text: Math.round(b0Slider.value) + "%" }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "B1" }
                            SimSwitch {
                                checked: true
                                onToggled: simulator.setBatteryPresent(1, checked)
                            }
                            SimCombo {
                                Layout.preferredWidth: 84
                                model: ["active", "idle", "asleep", "unknown"]
                                currentIndex: 1
                                onActivated: simulator.setBatteryState(1, currentText)
                            }
                            Slider {
                                id: b1Slider
                                Layout.fillWidth: true
                                from: 0; to: 100; value: 75; stepSize: 1
                                onMoved: simulator.setBatteryCharge(1, Math.round(value))
                            }
                            SimValue { text: Math.round(b1Slider.value) + "%" }
                        }

                        CollapsibleSection {
                            title: "Battery (deep)"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
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
                                            SimField {
                                                Layout.preferredWidth: 72
                                                text: "54000"
                                                onEditingFinished: simulator.setBatteryVoltage(slotCol.slot,
                                                                                               parseInt(text) || 0)
                                            }
                                            SimSubLabel { text: "I (mA)"; labelWidth: 40 }
                                            SimField {
                                                Layout.preferredWidth: 72
                                                text: "0"
                                                onEditingFinished: simulator.setBatteryCurrent(slotCol.slot,
                                                                                               parseInt(text) || 0)
                                            }
                                            Item { Layout.fillWidth: true }
                                        }
                                        SimSliderRow {
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

                        CollapsibleSection {
                            title: "CB / Aux Battery"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text { text: "CB battery"; color: "#ccc"; font.pixelSize: 11; font.bold: true }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Present" }
                                    SimSwitch {
                                        checked: true
                                        onToggled: simulator.setCbBatteryField("present", checked ? "true" : "false")
                                    }
                                    SimSubLabel { text: "Charge"; labelWidth: 44 }
                                    SimField {
                                        Layout.preferredWidth: 56
                                        text: "95"
                                        onEditingFinished: simulator.setCbBatteryField("charge", text)
                                    }
                                    SimSubLabel { text: "Temp" }
                                    SimField {
                                        Layout.preferredWidth: 56
                                        text: "23"
                                        onEditingFinished: simulator.setCbBatteryField("temperature", text)
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                                Text { text: "Aux battery"; color: "#ccc"; font.pixelSize: 11; font.bold: true }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Voltage" }
                                    SimField {
                                        Layout.preferredWidth: 72
                                        text: "12500"; placeholderText: "mV"
                                        onEditingFinished: simulator.setAuxBatteryField("voltage", text)
                                    }
                                    SimSubLabel { text: "Charge"; labelWidth: 44 }
                                    SimField {
                                        Layout.preferredWidth: 56
                                        text: "100"
                                        onEditingFinished: simulator.setAuxBatteryField("charge", text)
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Status" }
                                    SimButton { text: "not-charging"; small: true; fixedWidth: 74
                                        onClicked: simulator.setAuxBatteryField("charge-status", "not-charging") }
                                    SimButton { text: "bulk"; small: true; fixedWidth: 74
                                        onClicked: simulator.setAuxBatteryField("charge-status", "bulk-charge") }
                                    SimButton { text: "absorb"; small: true; fixedWidth: 74
                                        onClicked: simulator.setAuxBatteryField("charge-status", "absorption-charge") }
                                    SimButton { text: "float"; small: true; fixedWidth: 74
                                        onClicked: simulator.setAuxBatteryField("charge-status", "float-charge") }
                                }
                            }
                        }

                        CollapsibleSection {
                            title: "Auto-Lock"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Trigger" }
                                    SimButton { text: "60s"; small: true; fixedWidth: 52
                                        onClicked: simulator.setAutoStandbyDeadline(60) }
                                    SimButton { text: "30s"; small: true; fixedWidth: 52
                                        onClicked: simulator.setAutoStandbyDeadline(30) }
                                    SimButton { text: "10s"; small: true; fixedWidth: 52
                                        onClicked: simulator.setAutoStandbyDeadline(10) }
                                    SimButton { text: "Clear"; small: true; fixedWidth: 52; color: "#f44336"
                                        onClicked: simulator.clearAutoStandbyDeadline() }
                                    Item { Layout.fillWidth: true }
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
                            title: "Dashboard / Theme service"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Debug" }
                                    SimButton { text: "Off"; small: true; fixedWidth: 68
                                        onClicked: simulator.setDebugOverlay("off") }
                                    SimButton { text: "Overlay"; small: true; fixedWidth: 68
                                        onClicked: simulator.setDebugOverlay("overlay") }
                                    Item { Layout.fillWidth: true }
                                }
                                SimSliderRow {
                                    label: "Bright"
                                    from: 0; to: 1500; value: 200; unit: "lx"; decimals: 0
                                    onMoved: function(v) { simulator.setBrightness(v) }
                                }
                            }
                        }
                    }

                    SimPane {
                        SectionHeader { text: "Nav availability" }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            SimLabel { text: "Maps" }
                            SimSwitch {
                                id: mapsAvailSwitch
                                checked: typeof navAvailabilityService !== "undefined"
                                         && navAvailabilityService.localDisplayMapsAvailable
                                onToggled: {
                                    if (typeof navAvailabilityService !== "undefined")
                                        navAvailabilityService.setOverride(checked, routingAvailSwitch.checked)
                                }
                            }
                            SimLabel { text: "Routing" }
                            SimSwitch {
                                id: routingAvailSwitch
                                checked: typeof navAvailabilityService !== "undefined"
                                         && navAvailabilityService.routingAvailable
                                onToggled: {
                                    if (typeof navAvailabilityService !== "undefined")
                                        navAvailabilityService.setOverride(mapsAvailSwitch.checked, checked)
                                }
                            }
                            Item { Layout.fillWidth: true }
                            SimButton {
                                text: "Auto"; small: true; color: "#666"; fixedWidth: 44
                                onClicked: {
                                    if (typeof navAvailabilityService !== "undefined")
                                        navAvailabilityService.clearOverride()
                                }
                            }
                        }

                        SectionHeader {
                            text: "Routes"
                            SimButton {
                                text: "Clear"; small: true; color: "#f44336"; fixedWidth: 44
                                onClicked: {
                                    if (typeof navigationService !== "undefined")
                                        navigationService.clearNavigation()
                                }
                            }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 4
                            rowSpacing: 4
                            SimButton { text: "C-burg → Moabit"; small: true; Layout.fillWidth: true; onClicked: simulator.loadTestRoute(1) }
                            SimButton { text: "Mitte → Moabit"; small: true; Layout.fillWidth: true; onClicked: simulator.loadTestRoute(2) }
                            SimButton { text: "Tempelhof → F'hain"; small: true; Layout.fillWidth: true; onClicked: simulator.loadTestRoute(3) }
                            SimButton { text: "Short"; small: true; Layout.fillWidth: true; onClicked: simulator.loadTestRoute(4) }
                            SimButton { text: "Roundabout"; small: true; Layout.fillWidth: true; onClicked: simulator.loadTestRoute(5) }
                            SimButton { text: "U-turn start"; small: true; Layout.fillWidth: true; onClicked: simulator.loadTestRoute(6) }
                        }

                        SectionHeader { text: "GPS" }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Freeze" }
                            SimSwitch {
                                checked: simulator.gpsFrozen
                                onToggled: {
                                    simulator.gpsFrozen = checked
                                    if (typeof mapService !== "undefined")
                                        mapService.deadReckoningPaused = checked
                                }
                            }
                            SimSubLabel { text: "State" }
                            SimCombo {
                                Layout.fillWidth: true
                                model: ["fix-established", "searching", "off", "error"]
                                currentIndex: 0
                                onActivated: simulator.setGpsState(currentText)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Lat" }
                            SimField {
                                id: latField
                                Layout.fillWidth: true
                                text: "52.520008"
                                onEditingFinished: simulator.setGpsPosition(
                                    parseFloat(text), parseFloat(lngField.text))
                            }
                            SimSubLabel { text: "Lng" }
                            SimField {
                                id: lngField
                                Layout.fillWidth: true
                                text: "13.404954"
                                onEditingFinished: simulator.setGpsPosition(
                                    parseFloat(latField.text), parseFloat(text))
                            }
                        }
                        SimSliderRow {
                            label: "Course"
                            from: 0; to: 359; value: 0; unit: "°"; decimals: 0
                            onMoved: function(v) { simulator.setGpsCourse(v) }
                        }

                        CollapsibleSection {
                            title: "GPS (deep)"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
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
                                    spacing: 4
                                    SimLabel { text: "Sats" }
                                    SimSubLabel { text: "used" }
                                    SimField {
                                        id: satsUsedField
                                        Layout.preferredWidth: 56
                                        text: "8"
                                    }
                                    SimSubLabel { text: "vis" }
                                    SimField {
                                        id: satsVisField
                                        Layout.preferredWidth: 56
                                        text: "12"
                                    }
                                    SimButton {
                                        text: "Set"; small: true; fixedWidth: 44
                                        onClicked: simulator.setGpsSatellites(parseInt(satsUsedField.text) || 0,
                                                                              parseInt(satsVisField.text) || 0)
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Field" }
                                    SimField {
                                        id: gpsFieldName
                                        Layout.preferredWidth: 100
                                        placeholderText: "field"
                                    }
                                    SimField {
                                        id: gpsFieldValue
                                        Layout.fillWidth: true
                                        placeholderText: "value"
                                    }
                                    SimButton {
                                        text: "Set"; small: true; fixedWidth: 44
                                        onClicked: simulator.setGpsField(gpsFieldName.text, gpsFieldValue.text)
                                    }
                                }
                            }
                        }

                        CollapsibleSection {
                            title: "Speed Limit / Road"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Limit" }
                                    SimField {
                                        Layout.preferredWidth: 72
                                        text: "50"; placeholderText: "km/h"
                                        onEditingFinished: simulator.setSpeedLimit(text)
                                    }
                                    SimSubLabel { text: "Type" }
                                    SimCombo {
                                        Layout.fillWidth: true
                                        model: ["", "residential", "secondary", "primary", "motorway",
                                                "tertiary", "service", "footway"]
                                        onActivated: simulator.setRoadType(currentText)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Name" }
                                    SimField {
                                        Layout.fillWidth: true
                                        text: "Alexanderplatz"
                                        onEditingFinished: simulator.setRoadName(text)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Refs" }
                                    SimField {
                                        Layout.fillWidth: true
                                        placeholderText: "B 96"
                                        onEditingFinished: simulator.setRoadRefs(text)
                                    }
                                }
                            }
                        }

                        SectionHeader { text: "OTA" }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "MDB" }
                            SimCombo {
                                Layout.fillWidth: true
                                model: ["idle", "downloading", "preparing", "installing",
                                        "pending-reboot", "error"]
                                currentIndex: 0
                                onActivated: simulator.setOtaStatus("mdb", currentText)
                            }
                            SimSubLabel { text: "DBC" }
                            SimCombo {
                                Layout.fillWidth: true
                                model: ["idle", "downloading", "preparing", "installing",
                                        "pending-reboot", "error"]
                                currentIndex: 0
                                onActivated: simulator.setOtaStatus("dbc", currentText)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Method" }
                            ButtonGroup { id: otaMethodMdbGroup; exclusive: true }
                            SimButton {
                                text: "MDB delta"; small: true; fixedWidth: 74
                                ButtonGroup.group: otaMethodMdbGroup
                                checkable: true
                                onClicked: simulator.setOtaUpdateMethod("mdb", "delta")
                            }
                            SimButton {
                                text: "MDB full"; small: true; fixedWidth: 74
                                ButtonGroup.group: otaMethodMdbGroup
                                checkable: true
                                onClicked: simulator.setOtaUpdateMethod("mdb", "full")
                            }
                            Item { Layout.preferredWidth: 8 }
                            ButtonGroup { id: otaMethodDbcGroup; exclusive: true }
                            SimButton {
                                text: "DBC delta"; small: true; fixedWidth: 74
                                ButtonGroup.group: otaMethodDbcGroup
                                checkable: true
                                onClicked: simulator.setOtaUpdateMethod("dbc", "delta")
                            }
                            SimButton {
                                text: "DBC full"; small: true; fixedWidth: 74
                                ButtonGroup.group: otaMethodDbcGroup
                                checkable: true
                                onClicked: simulator.setOtaUpdateMethod("dbc", "full")
                            }
                            Item { Layout.fillWidth: true }
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
                            SimValue { text: Math.round(otaDlSlider.value) + "%" }
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
                            SimValue { text: Math.round(otaInstSlider.value) + "%" }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Version" }
                            SimButton {
                                text: "Set Versions"; small: true; fixedWidth: 88
                                onClicked: {
                                    simulator.setOtaUpdateVersion("mdb", "v0.99.0")
                                    simulator.setOtaUpdateVersion("dbc", "v0.99.0")
                                }
                            }
                            SimButton {
                                text: "Reset"; small: true; color: "#666"; fixedWidth: 60
                                onClicked: simulator.clearOta()
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Error" }
                            SimCombo {
                                id: otaErrorCombo
                                Layout.fillWidth: true
                                model: ["file-not-found", "invalid-file", "download-failed",
                                        "install-failed", "reboot-failed", "delta-failed"]
                                currentIndex: 2
                            }
                            SimButton {
                                text: "Trigger Error (MDB)"; small: true; color: "#ff9800"; fixedWidth: 110
                                onClicked: {
                                    simulator.setOtaError("mdb", otaErrorCombo.currentText)
                                    simulator.setOtaErrorMessage("mdb", "Simulated " + otaErrorCombo.currentText)
                                }
                            }
                        }

                        SectionHeader { text: "Connectivity" }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Modem" }
                            SimCombo {
                                Layout.fillWidth: true
                                model: ["connected", "disconnected", "off"]
                                currentIndex: 0
                                onActivated: simulator.setModemState(currentText)
                            }
                            SimSubLabel { text: "Cloud"; labelWidth: 40 }
                            SimButton {
                                text: "On"; small: true; fixedWidth: 40
                                onClicked: simulator.setCloudConnection("connected")
                            }
                            SimButton {
                                text: "Off"; small: true; fixedWidth: 40; color: "#f44336"
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
                                    text: modelData; small: true; fixedWidth: 42
                                    ButtonGroup.group: techGroup
                                    checkable: true; checked: index === 0
                                    onClicked: simulator.setAccessTech(modelData)
                                }
                            }
                            Item { Layout.preferredWidth: 8 }
                            SimSubLabel { text: "BT" }
                            SimButton {
                                text: "On"; small: true; fixedWidth: 40
                                onClicked: simulator.setBluetoothStatus("connected")
                            }
                            SimButton {
                                text: "Off"; small: true; fixedWidth: 40; color: "#f44336"
                                onClicked: simulator.setBluetoothStatus("disconnected")
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "USB" }
                            SimButton {
                                text: "Disconnect"; small: true; fixedWidth: 68; color: "#f44336"
                                onClicked: connectionStore.simulateUsbDisconnect(true)
                            }
                            SimButton {
                                text: "Reconnect"; small: true; fixedWidth: 68; color: "#4caf50"
                                onClicked: connectionStore.simulateUsbDisconnect(false)
                            }
                            Item { Layout.preferredWidth: 8 }
                            SimSubLabel { text: "UMS" }
                            SimButton {
                                text: "Activate"; small: true; fixedWidth: 56; color: "#2196F3"
                                onClicked: { simulator.setUsbStatus("active"); simulator.setUsbMode("ums") }
                            }
                            SimButton {
                                text: "Exit"; small: true; fixedWidth: 56; color: "#f44336"
                                onClicked: { simulator.setUsbStatus("idle"); simulator.setUsbMode("normal") }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        CollapsibleSection {
                            title: "Bluetooth (deep)"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "MAC" }
                                    SimField {
                                        id: btMacField
                                        Layout.fillWidth: true
                                        text: "AA:BB:CC:DD:EE:FF"
                                        onEditingFinished: simulator.setBluetoothMac(text)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "PIN" }
                                    SimField {
                                        id: btPinField
                                        Layout.fillWidth: true
                                        placeholderText: "(empty = no pairing)"
                                        onEditingFinished: simulator.setBluetoothPin(text)
                                    }
                                    SimButton {
                                        text: "123456"; small: true; fixedWidth: 56
                                        onClicked: { btPinField.text = "123456"; simulator.setBluetoothPin("123456") }
                                    }
                                    SimButton {
                                        text: "Clear"; small: true; color: "#666"; fixedWidth: 44
                                        onClicked: { btPinField.text = ""; simulator.setBluetoothPin("") }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Health" }
                                    SimButton { text: "ok"; small: true; fixedWidth: 56
                                        onClicked: simulator.setBluetoothServiceHealth("ok") }
                                    SimButton { text: "warn"; small: true; fixedWidth: 56
                                        onClicked: simulator.setBluetoothServiceHealth("warn") }
                                    SimButton { text: "error"; small: true; fixedWidth: 56; color: "#f44336"
                                        onClicked: simulator.setBluetoothServiceHealth("error") }
                                    Item { Layout.fillWidth: true }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Error" }
                                    SimField {
                                        id: btErrField
                                        Layout.fillWidth: true
                                        placeholderText: "service-error string"
                                        onEditingFinished: simulator.setBluetoothServiceError(text)
                                    }
                                }
                            }
                        }

                        CollapsibleSection {
                            title: "Internet (deep)"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "IP" }
                                    SimField {
                                        Layout.fillWidth: true
                                        text: "10.0.0.42"
                                        onEditingFinished: simulator.setIpAddress(text)
                                    }
                                    SimLabel { text: "IMEI" }
                                    SimField {
                                        Layout.fillWidth: true
                                        text: "351756051523999"
                                        onEditingFinished: simulator.setSimImei(text)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "IMSI" }
                                    SimField {
                                        Layout.fillWidth: true
                                        text: "262011000000000"
                                        onEditingFinished: simulator.setSimImsi(text)
                                    }
                                    SimLabel { text: "ICCID" }
                                    SimField {
                                        Layout.fillWidth: true
                                        text: "8949010000000000000"
                                        onEditingFinished: simulator.setSimIccid(text)
                                    }
                                }
                            }
                        }

                        CollapsibleSection {
                            title: "USB / UMS (deep)"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Step" }
                                    SimField {
                                        Layout.fillWidth: true
                                        placeholderText: "step name"
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
                                    spacing: 4
                                    SimLabel { text: "Detail" }
                                    SimField {
                                        Layout.fillWidth: true
                                        placeholderText: "detail line"
                                        onEditingFinished: simulator.setUsbDetail(text)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "Log" }
                                    SimField {
                                        id: umsLogField
                                        Layout.fillWidth: true
                                        placeholderText: "log line"
                                    }
                                    SimButton {
                                        text: "Push"; small: true; fixedWidth: 44
                                        onClicked: { simulator.pushUmsLog(umsLogField.text); umsLogField.text = "" }
                                    }
                                }
                            }
                        }

                        CollapsibleSection {
                            title: "System / Versions"
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "MDB" }
                                    SimField {
                                        Layout.fillWidth: true
                                        text: "stable-v1.0.0"
                                        onEditingFinished: simulator.setMdbVersion(text)
                                        Component.onCompleted: simulator.setMdbVersion(text)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "DBC" }
                                    SimField {
                                        Layout.fillWidth: true
                                        text: "stable-v1.0.0"
                                        onEditingFinished: simulator.setDbcVersion(text)
                                        Component.onCompleted: simulator.setDbcVersion(text)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    SimLabel { text: "nRF" }
                                    SimField {
                                        Layout.fillWidth: true
                                        text: "v2.3.0-ls"
                                        onEditingFinished: simulator.setSystemField("nrf-fw-version", text)
                                        Component.onCompleted: simulator.setSystemField("nrf-fw-version", text)
                                    }
                                }
                            }
                        }
                    }
                }

                SectionHeader { text: "Overrides" }

                // Trip block - edit any two of {Duration, Avg, Trip distance};
                // the third recomputes. Most-recently-edited two are kept; the
                // stale one is overwritten. The fields have to stay direct
                // children of this row, they reach noteEdit() through parent.
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
                        if (typeof tripStore !== "undefined")
                            tripStore.setOverride(dist, Math.round(dur), avg)
                    }

                    SimSubLabel { text: "clock" }
                    SimField {
                        id: clockOverrideField
                        Layout.preferredWidth: 64
                        placeholderText: "HH:mm"
                        text: simulator.clockOverride
                        onEditingFinished: simulator.clockOverride = text
                    }
                    SimSubLabel { text: "date" }
                    SimField {
                        id: dateOverrideField
                        Layout.preferredWidth: 92
                        placeholderText: "yyyy-MM-dd"
                        text: simulator.dateOverride
                        onEditingFinished: simulator.dateOverride = text
                    }

                    Item { Layout.preferredWidth: 12 }

                    SimSubLabel { text: "dur s" }
                    SimField {
                        id: tripDurField
                        Layout.preferredWidth: 64
                        text: "1830"
                        onEditingFinished: parent.noteEdit("dur")
                    }
                    SimSubLabel { text: "avg" }
                    SimField {
                        id: tripAvgField
                        Layout.preferredWidth: 56
                        text: "24.2"
                        onEditingFinished: parent.noteEdit("avg")
                    }
                    SimSubLabel { text: "trip" }
                    SimField {
                        id: tripDistField
                        Layout.preferredWidth: 56
                        text: "12.3"
                        onEditingFinished: parent.noteEdit("dist")
                    }
                    SimSubLabel { text: "total" }
                    SimField {
                        id: odometerOverrideField
                        Layout.preferredWidth: 64
                        text: "0.0"
                        onEditingFinished: {
                            var v = parseFloat(text)
                            if (!isNaN(v)) simulator.setOdometer(v)
                        }
                        Component.onCompleted: simulator.setOdometer(0)
                    }

                    Item { Layout.fillWidth: true }

                    SimSubLabel { text: "freeze" }
                    SimSwitch {
                        id: freezeTripSwitch
                        checked: false
                        ToolTip.text: "Freeze trip timer"
                        ToolTip.visible: hovered
                        onToggled: {
                            if (typeof tripStore === "undefined") return
                            if (checked) {
                                tripStore.setOverride(tripStore.distance,
                                                      tripStore.duration,
                                                      tripStore.averageSpeed)
                            } else {
                                tripStore.clearOverride()
                            }
                        }
                    }
                    SimButton {
                        text: "Clear"; small: true; color: "#666"; fixedWidth: 44
                        onClicked: {
                            clockOverrideField.text = ""
                            simulator.clockOverride = ""
                            dateOverrideField.text = ""
                            simulator.dateOverride = ""
                            if (typeof tripStore !== "undefined")
                                tripStore.clearOverride()
                            freezeTripSwitch.checked = false
                        }
                    }
                }

                CollapsibleSection {
                    title: "Settings (visibility / alarm / blinker)"
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        // Two visibility rows per line; a row is only ~430 px wide,
                        // so the full width fits a pair without crowding.
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 16
                            rowSpacing: 4
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
                                    SimLabel {
                                        text: visRow.modelData.label
                                        labelWidth: 80
                                    }
                                    ButtonGroup { id: visGroup }
                                    Repeater {
                                        model: ["always", "active-or-error", "error", "never"]
                                        SimButton {
                                            required property string modelData
                                            text: modelData
                                            small: true
                                            fixedWidth: 84
                                            ButtonGroup.group: visGroup
                                            checkable: true
                                            onClicked: simulator.setSetting(visRow.settingKey, modelData)
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Blinker" }
                            SimButton { text: "Icon"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("dashboard.blinker-style", "icon") }
                            SimButton { text: "Overlay"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("dashboard.blinker-style", "overlay") }
                            Item { Layout.preferredWidth: 12 }
                            SimLabel { text: "DBC LED" }
                            SimButton { text: "On"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("scooter.dbc-blinker-led", "enabled") }
                            SimButton { text: "Off"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("scooter.dbc-blinker-led", "disabled") }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Alarm" }
                            SimButton { text: "Enable"; small: true; fixedWidth: 68; color: "#4caf50"
                                onClicked: simulator.setSetting("alarm.enabled", "true") }
                            SimButton { text: "Disable"; small: true; fixedWidth: 68; color: "#f44336"
                                onClicked: simulator.setSetting("alarm.enabled", "false") }
                            SimButton { text: "Honk"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("alarm.honk", "true") }
                            SimButton { text: "10s"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("alarm.duration", "10") }
                            SimButton { text: "30s"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("alarm.duration", "30") }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Battery" }
                            SimButton { text: "Single"; small: true; fixedWidth: 68
                                onClicked: simulator.setDualBattery(false) }
                            SimButton { text: "Dual"; small: true; fixedWidth: 68
                                onClicked: simulator.setDualBattery(true) }
                            Item { Layout.preferredWidth: 12 }
                            SimLabel { text: "Map" }
                            SimButton { text: "Online"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("dashboard.map.type", "online") }
                            SimButton { text: "Offline"; small: true; fixedWidth: 68
                                onClicked: simulator.setSetting("dashboard.map.type", "offline") }
                            Item { Layout.preferredWidth: 12 }
                            SimLabel { text: "Traffic" }
                            SimSwitch {
                                checked: false
                                onToggled: simulator.setTrafficOverlay(checked)
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                CollapsibleSection {
                    title: "Raw Redis Injection"
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            Layout.fillWidth: true
                            text: "Direct (channel, field) hash setter - escape hatch for anything not exposed above."
                            color: "#888"; font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Hash" }
                            SimField {
                                id: rawChannel
                                Layout.preferredWidth: 140
                                placeholderText: "channel"
                            }
                            SimField {
                                id: rawField
                                Layout.preferredWidth: 140
                                placeholderText: "field"
                            }
                            SimField {
                                id: rawValue
                                Layout.fillWidth: true
                                placeholderText: "value"
                            }
                            SimButton {
                                text: "Set"; small: true; color: "#2196F3"; fixedWidth: 68
                                onClicked: simulator.setRaw(rawChannel.text, rawField.text, rawValue.text)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            SimLabel { text: "Send" }
                            SimField {
                                id: pubChannel
                                Layout.preferredWidth: 140
                                placeholderText: "channel"
                            }
                            SimField {
                                id: pubMessage
                                Layout.fillWidth: true
                                placeholderText: "pub/sub message"
                            }
                            SimButton {
                                text: "Publish"; small: true; fixedWidth: 68
                                onClicked: simulator.publishMessage(pubChannel.text, pubMessage.text)
                            }
                            SimButton {
                                text: "LPush"; small: true; fixedWidth: 68
                                onClicked: simulator.pushList(pubChannel.text, pubMessage.text)
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 16 }
            }
        }
    }

    // Two independent running columns. Each pane is a continuous flow of
    // sections anchored to the top, so a section that grows pushes only the
    // rest of its own column down and never leaves a hole beside it. Equal
    // preferred widths keep the split even whatever the panes contain.
    component SimSplit: RowLayout {
        Layout.fillWidth: true
        spacing: 12
    }

    component SimPane: ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: false
        Layout.preferredWidth: 1
        Layout.alignment: Qt.AlignTop
        spacing: 4
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
        Layout.topMargin: 8
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
            spacing: 4
            visible: section.expanded
        }
    }

    // Leading label of a row. The width is pinned (min == preferred == max) so
    // every row in a block starts its controls at the same x.
    component SimLabel: Text {
        property int labelWidth: 60
        color: "#999"
        font.pixelSize: 10
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        Layout.minimumWidth: labelWidth
        Layout.preferredWidth: labelWidth
        Layout.maximumWidth: labelWidth
    }

    // Label for a second group further along the same row. Right-aligned so it
    // sticks to the controls it belongs to.
    component SimSubLabel: Text {
        property int labelWidth: 34
        color: "#999"
        font.pixelSize: 9
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        Layout.minimumWidth: labelWidth
        Layout.preferredWidth: labelWidth
        Layout.maximumWidth: labelWidth
    }

    // Trailing numeric readout of a slider row.
    component SimValue: Text {
        color: "#ccc"
        font.pixelSize: 11
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        Layout.minimumWidth: 48
        Layout.preferredWidth: 48
        Layout.maximumWidth: 48
    }

    component SimButton: Button {
        id: btn
        property bool small: false
        property color color: "#555"
        // Pins the width so a run of buttons reads as one block; -1 falls back
        // to the implicit text width.
        property int fixedWidth: -1
        Layout.fillWidth: !small && fixedWidth < 0
        Layout.preferredWidth: fixedWidth
        Layout.minimumWidth: fixedWidth > 0 ? fixedWidth : (small ? 32 : 56)
        Layout.preferredHeight: small ? 22 : 26
        padding: small ? 4 : 6
        font.pixelSize: small ? 10 : 11
        background: Rectangle {
            color: btn.down ? Qt.lighter(btn.color, 1.3)
                   : btn.checked ? "#2196F3"
                   : btn.color
            radius: 4
        }
        contentItem: Text {
            text: btn.text
            color: "white"
            font: btn.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component SimField: TextField {
        color: "white"
        font.pixelSize: 11
        leftPadding: 6
        rightPadding: 6
        topPadding: 3
        bottomPadding: 3
        Layout.preferredHeight: 24
        background: Rectangle { color: "#333"; radius: 3 }
    }

    component SimCombo: ComboBox {
        font.pixelSize: 10
        Layout.preferredHeight: 24
        palette.button: "#333"; palette.buttonText: "white"
        palette.window: "#333"; palette.windowText: "white"
        palette.highlight: "#2196F3"
    }

    // Scaled from the left edge: a full-size Switch is ~68 px wide, which is
    // more than a dense debug row can spare.
    component SimSwitch: Switch {
        scale: 0.7
        transformOrigin: Item.Left
        palette.highlight: "#2196F3"
        Layout.minimumWidth: 48
        Layout.preferredWidth: 48
        Layout.preferredHeight: 26
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
        SimValue {
            text: (decimals === 0 ? Math.round(slider.value) : slider.value.toFixed(decimals)) + unit
        }
    }
}

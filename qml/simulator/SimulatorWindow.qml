import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScootUI 1.0

ApplicationWindow {
    id: simWindow
    title: "ScootUI Simulator"
    width: 480
    height: 800
    visible: true
    color: "#1e1e1e"

    // Position to the right of the main dashboard window
    x: Screen.width / 2 - (480 + width + 20) / 2 + 480 + 20
    y: Screen.height / 2 - 480 / 2

    ScrollView {
        anchors.fill: parent
        anchors.margins: 8
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 8

            SimButton {
                text: "Screenshot"
                Layout.fillWidth: true
                color: "#607D8B"
                onClicked: SimulatorService.takeScreenshot()
            }

            // ---- Screen ----
            SectionHeader { text: "Screen" }

            RowLayout {
                Layout.fillWidth: true
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
                    text: "Debug"; small: true
                    ButtonGroup.group: screenGroup
                    checkable: true
                    onClicked: ScreenStore.setScreen(3)
                }
                SimButton {
                    text: "About"; small: true
                    ButtonGroup.group: screenGroup
                    checkable: true
                    onClicked: ScreenStore.setScreen(4)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Theme" }
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
                Item { Layout.preferredWidth: 8 }
                SimLabel { text: "Lang" }
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

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Traffic" }
                Switch {
                    checked: false
                    palette.highlight: "#2196F3"
                    onToggled: SimulatorService.setTrafficOverlay(checked)
                }
            }

            // ---- Connection ----
            SectionHeader { text: "Connection" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "USB" }
                SimButton {
                    text: "Disconnect"
                    color: "#f44336"
                    onClicked: ConnectionStore.simulateUsbDisconnect(true)
                }
                SimButton {
                    text: "Reconnect"
                    color: "#4caf50"
                    onClicked: ConnectionStore.simulateUsbDisconnect(false)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "UMS" }
                SimButton {
                    text: "Activate"
                    color: "#2196F3"
                    onClicked: {
                        SimulatorService.setUsbStatus("active")
                        SimulatorService.setUsbMode("ums")
                    }
                }
                SimButton {
                    text: "Processing"
                    color: "#ff9800"
                    onClicked: {
                        SimulatorService.setUsbStatus("processing")
                    }
                }
                SimButton {
                    text: "Exit"
                    color: "#f44336"
                    onClicked: {
                        SimulatorService.setUsbStatus("idle")
                        SimulatorService.setUsbMode("normal")
                    }
                }
            }

            // ---- Presets ----
            SectionHeader { text: "Presets" }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 4
                rowSpacing: 4

                SimButton { text: "Parked"; onClicked: SimulatorService.loadPreset("parked") }
                SimButton { text: "Ready"; onClicked: SimulatorService.loadPreset("ready") }
                SimButton { text: "Driving"; onClicked: SimulatorService.loadPreset("driving") }
                SimButton { text: "Fast"; onClicked: SimulatorService.loadPreset("driving-fast") }
                SimButton { text: "Low Batt"; color: "#ff6b35"; onClicked: SimulatorService.loadPreset("low-battery") }
                SimButton { text: "Charging"; color: "#4caf50"; onClicked: SimulatorService.loadPreset("charging") }
                SimButton { text: "No GPS"; onClicked: SimulatorService.loadPreset("no-gps") }
                SimButton { text: "Offline"; onClicked: SimulatorService.loadPreset("offline") }
                SimButton { text: "1 Battery"; onClicked: SimulatorService.loadPreset("single-battery") }
            }

            // ---- Routes ----
            SectionHeader { text: "Routes (Polyline Test)" }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 4
                rowSpacing: 4
                SimButton {
                    text: "Charlottenburg \u2192 Moabit"
                    Layout.fillWidth: true
                    onClicked: SimulatorService.loadTestRoute(1)
                }
                SimButton {
                    text: "Mitte \u2192 Moabit"
                    Layout.fillWidth: true
                    onClicked: SimulatorService.loadTestRoute(2)
                }
                SimButton {
                    text: "Tempelhof \u2192 Friedrichshain"
                    Layout.fillWidth: true
                    onClicked: SimulatorService.loadTestRoute(3)
                }
                SimButton {
                    text: "Clear Route"
                    Layout.fillWidth: true
                    color: "#f44336"
                    onClicked: {
                        if (true) {
                            NavigationService.clearNavigation()
                        }
                    }
                }
            }

            // ---- Vehicle ----
            SectionHeader { text: "Vehicle" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "State" }
                ComboBox {
                    id: vehicleStateCombo
                    Layout.fillWidth: true
                    model: ["parked", "ready-to-drive", "stand-by", "off",
                            "shutting-down", "booting", "hibernating",
                            "waiting-hibernation", "updating"]
                    currentIndex: 0
                    onActivated: SimulatorService.setVehicleState(currentText)

                    palette.button: "#333"
                    palette.buttonText: "white"
                    palette.window: "#333"
                    palette.windowText: "white"
                    palette.highlight: "#2196F3"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Kickstand" }
                Switch {
                    id: kickstandSwitch
                    checked: true
                    text: checked ? "Down" : "Up"
                    palette.highlight: "#2196F3"
                    onToggled: SimulatorService.setKickstand(checked ? "down" : "up")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Blinkers" }
                ButtonGroup { id: blinkerGroup; exclusive: true }
                SimButton {
                    text: "Off"; small: true
                    ButtonGroup.group: blinkerGroup
                    checkable: true; checked: true
                    onClicked: SimulatorService.setBlinkerState("off")
                }
                SimButton {
                    text: "L"; small: true
                    ButtonGroup.group: blinkerGroup
                    checkable: true
                    onClicked: SimulatorService.setBlinkerState("left")
                }
                SimButton {
                    text: "R"; small: true
                    ButtonGroup.group: blinkerGroup
                    checkable: true
                    onClicked: SimulatorService.setBlinkerState("right")
                }
                SimButton {
                    text: "Both"; small: true
                    ButtonGroup.group: blinkerGroup
                    checkable: true
                    onClicked: SimulatorService.setBlinkerState("both")
                }
            }

            // ---- Controls ----
            SectionHeader { text: "Controls" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Brakes" }
                SimButton {
                    text: "Left Brake"
                    onPressed: SimulatorService.setBrakeLeft(true)
                    onReleased: SimulatorService.setBrakeLeft(false)
                }
                SimButton {
                    text: "Right Brake"
                    onPressed: SimulatorService.setBrakeRight(true)
                    onReleased: SimulatorService.setBrakeRight(false)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Buttons" }
                SimButton {
                    text: "Seatbox"
                    onPressed: SimulatorService.setSeatboxButton(true)
                    onReleased: SimulatorService.setSeatboxButton(false)
                }
                SimButton {
                    text: "Horn"
                    color: "#ff9800"
                    onPressed: SimulatorService.setHornButton(true)
                    onReleased: SimulatorService.setHornButton(false)
                }
            }

            RowLayout {
                Layout.fillWidth: true
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

            // ---- Auto-Lock ----
            SectionHeader { text: "Auto-Lock" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Trigger" }
                SimButton {
                    text: "60s"; small: true
                    onClicked: SimulatorService.setAutoStandbyDeadline(60)
                }
                SimButton {
                    text: "30s"; small: true
                    onClicked: SimulatorService.setAutoStandbyDeadline(30)
                }
                SimButton {
                    text: "10s"; small: true
                    onClicked: SimulatorService.setAutoStandbyDeadline(10)
                }
                SimButton {
                    text: "Clear"; small: true; color: "#f44336"
                    onClicked: SimulatorService.clearAutoStandbyDeadline()
                }
            }

            SimSliderRow {
                label: "Timeout"
                from: 0; to: 1800; value: 900; unit: "s"; decimals: 0
                onMoved: function(v) { SimulatorService.setAutoStandbySetting(Math.round(v)) }
            }

            Text {
                Layout.fillWidth: true
                visible: AutoStandbyStore.remainingSeconds > 0
                text: "Remaining: " + AutoStandbyStore.remainingSeconds + "s"
                color: AutoStandbyStore.remainingSeconds <= 60 ? "#FF9800" : "#4caf50"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }

            // ---- Engine ----
            SectionHeader { text: "Engine" }

            SimSliderRow {
                label: "Speed"
                from: 0; to: 55; value: 0; unit: "km/h"; decimals: 0
                onMoved: function(v) { SimulatorService.setSpeed(v); SimulatorService.setGpsSpeed(v) }
            }

            SimSliderRow {
                label: "Temp"
                from: -10; to: 120; value: 25; unit: "\u00B0C"; decimals: 0
                onMoved: function(v) { SimulatorService.setEngineTemperature(v) }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Odometer"; Layout.preferredWidth: 100; color: "#CCC"; font.pixelSize: 12 }
                TextField {
                    Layout.fillWidth: true
                    text: "0.0"
                    placeholderText: "km"
                    font.pixelSize: 12
                    onEditingFinished: {
                        var v = parseFloat(text)
                        if (!isNaN(v)) SimulatorService.setOdometer(v)
                    }
                    Component.onCompleted: SimulatorService.setOdometer(0)
                }
                Text { text: "km"; color: "#999"; font.pixelSize: 12 }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Trip / Milestone"; Layout.preferredWidth: 100; color: "#CCC"; font.pixelSize: 12 }
                Text {
                    Layout.fillWidth: true
                    color: "#CCC"
                    font.pixelSize: 12
                    text: {
                        var trip = (TripStore.distance !== undefined)
                                    ? (TripStore.distance / 1000).toFixed(2) + " km" : "—"
                        var ms = (OdometerMilestoneService.currentKm !== undefined
                                  && OdometerMilestoneService.currentKm > 0)
                                    ? OdometerMilestoneService.currentKm.toFixed(1) + " km"
                                      + (OdometerMilestoneService.currentTag
                                         ? " [" + OdometerMilestoneService.currentTag + "]"
                                         : "")
                                    : "—"
                        return "trip " + trip + " · last " + ms
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Easter eggs"; Layout.preferredWidth: 100; color: "#CCC"; font.pixelSize: 12 }
                CheckBox {
                    checked: OdometerMilestoneService.easterEggsEnabled
                    onToggled: {
                        if (true)
                            OdometerMilestoneService.easterEggsEnabled = checked
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: "666, 1024, 1234.5, 1337, 3133.7, 8008.5, 9999.9"
                    color: "#999"
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
            }

            SimSliderRow {
                label: "Motor Current"
                from: -10000; to: 80000; value: 0; unit: "mA"; decimals: 0
                onMoved: function(v) { SimulatorService.setMotorCurrent(v) }
            }

            SimSliderRow {
                label: "Motor Voltage"
                from: 0; to: 60000; value: 54000; unit: "mV"; decimals: 0
                onMoved: function(v) { SimulatorService.setMotorVoltage(v) }
            }

            // ---- Auto-drive ----
            SectionHeader { text: "Auto-Drive" }

            RowLayout {
                Layout.fillWidth: true
                SimButton {
                    text: SimulatorService.autoDriveActive ? "Stop" : "Start"
                    color: SimulatorService.autoDriveActive ? "#f44336" : "#4caf50"
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
                    color: "#ccc"
                    font.pixelSize: 11
                    Layout.preferredWidth: 48
                }
            }

            Text {
                visible: SimulatorService.autoDriveActive
                text: "Driving at " + SimulatorService.autoDriveSpeed.toFixed(1) + " km/h"
                color: "#4caf50"
                font.pixelSize: 12
                Layout.alignment: Qt.AlignHCenter
            }

            // ---- Battery 0 ----
            SectionHeader { text: "Battery 0" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Present" }
                Switch {
                    checked: true
                    palette.highlight: "#2196F3"
                    onToggled: SimulatorService.setBatteryPresent(0, checked)
                }
                SimLabel { text: "State" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["active", "idle", "asleep", "unknown"]
                    currentIndex: 0
                    onActivated: SimulatorService.setBatteryState(0, currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
            }

            SimSliderRow {
                label: "Charge"
                from: 0; to: 100; value: 80; unit: "%"; decimals: 0
                onMoved: function(v) { SimulatorService.setBatteryCharge(0, Math.round(v)) }
            }

            SimSliderRow {
                label: "Temp"
                from: -20; to: 60; value: 25; unit: "\u00B0C"; decimals: 0
                onMoved: function(v) { SimulatorService.setBatteryTemperature(0, Math.round(v)) }
            }

            // ---- Battery 1 ----
            SectionHeader { text: "Battery 1" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Present" }
                Switch {
                    checked: true
                    palette.highlight: "#2196F3"
                    onToggled: SimulatorService.setBatteryPresent(1, checked)
                }
                SimLabel { text: "State" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["active", "idle", "asleep", "unknown"]
                    currentIndex: 0
                    onActivated: SimulatorService.setBatteryState(1, currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
            }

            SimSliderRow {
                label: "Charge"
                from: 0; to: 100; value: 75; unit: "%"; decimals: 0
                onMoved: function(v) { SimulatorService.setBatteryCharge(1, Math.round(v)) }
            }

            // ---- GPS ----
            SectionHeader { text: "GPS" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Freeze position" }
                Switch {
                    checked: SimulatorService.gpsFrozen
                    onToggled: {
                        SimulatorService.gpsFrozen = checked
                        if (true)
                            MapService.deadReckoningPaused = checked
                    }
                    palette.button: "#333"
                }
            }

            RowLayout {
                Layout.fillWidth: true
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
                SimLabel { text: "Lat" }
                TextField {
                    id: latField
                    Layout.fillWidth: true
                    text: "52.520008"
                    color: "white"
                    font.pixelSize: 12
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: SimulatorService.setGpsPosition(
                        parseFloat(text), parseFloat(lngField.text))
                }
                SimLabel { text: "Lng" }
                TextField {
                    id: lngField
                    Layout.fillWidth: true
                    text: "13.404954"
                    color: "white"
                    font.pixelSize: 12
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: SimulatorService.setGpsPosition(
                        parseFloat(latField.text), parseFloat(text))
                }
            }

            SimSliderRow {
                label: "Course"
                from: 0; to: 359; value: 0; unit: "\u00B0"; decimals: 0
                onMoved: function(v) { SimulatorService.setGpsCourse(v) }
            }

            // ---- Internet ----
            SectionHeader { text: "Internet" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Modem" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["connected", "disconnected", "off"]
                    currentIndex: 0
                    onActivated: SimulatorService.setModemState(currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
            }

            SimSliderRow {
                label: "Signal"
                from: 0; to: 100; value: 75; unit: "%"; decimals: 0
                onMoved: function(v) { SimulatorService.setSignalQuality(Math.round(v)) }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Tech" }
                ButtonGroup { id: techGroup; exclusive: true }
                Repeater {
                    model: ["LTE", "UMTS", "EDGE", "GSM"]
                    SimButton {
                        required property var modelData
                        required property int index
                        text: modelData; small: true
                        ButtonGroup.group: techGroup
                        checkable: true; checked: index === 0
                        onClicked: SimulatorService.setAccessTech(modelData)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Cloud" }
                SimButton {
                    text: "Connected"; small: true
                    onClicked: SimulatorService.setCloudConnection("connected")
                }
                SimButton {
                    text: "Disconnected"; small: true
                    onClicked: SimulatorService.setCloudConnection("disconnected")
                }
            }

            // ---- Bluetooth ----
            SectionHeader { text: "Bluetooth" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Status" }
                SimButton {
                    text: "Connected"; small: true
                    onClicked: SimulatorService.setBluetoothStatus("connected")
                }
                SimButton {
                    text: "Disconnected"; small: true
                    onClicked: SimulatorService.setBluetoothStatus("disconnected")
                }
            }


            // Bottom spacer
            Item { Layout.preferredHeight: 20 }
        }
    }

    // ---- Inline component definitions ----

    component SectionHeader: Rectangle {
        property alias text: headerText.text
        Layout.fillWidth: true
        Layout.topMargin: 4
        height: 24
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

    component SimLabel: Text {
        color: "#999"
        font.pixelSize: 12
        Layout.preferredWidth: 52
    }

    component SimButton: Button {
        id: simBtn
        property bool small: false
        property color color: "#555"
        Layout.fillWidth: !simBtn.small
        Layout.preferredWidth: simBtn.small ? 48 : -1
        Layout.preferredHeight: simBtn.small ? 28 : 32
        font.pixelSize: simBtn.small ? 11 : 12
        background: Rectangle {
            color: simBtn.down ? Qt.lighter(simBtn.color, 1.3)
                   : simBtn.checked ? "#2196F3"
                   : simBtn.color
            radius: 4
        }
        contentItem: Text {
            text: simBtn.text
            color: "white"
            font: simBtn.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component SimSliderRow: RowLayout {
        id: sliderRow
        property alias label: sliderLabel.text
        property alias from: slider.from
        property alias to: slider.to
        property alias value: slider.value
        property string unit: ""
        property int decimals: 1
        signal moved(real value)

        Layout.fillWidth: true
        SimLabel { id: sliderLabel }
        Slider {
            id: slider
            Layout.fillWidth: true
            stepSize: sliderRow.decimals === 0 ? 1 : 0.1
            onMoved: sliderRow.moved(value)
        }
        Text {
            text: (sliderRow.decimals === 0 ? Math.round(slider.value) : slider.value.toFixed(sliderRow.decimals)) + sliderRow.unit
            color: "#ccc"
            font.pixelSize: 11
            Layout.preferredWidth: 52
        }
    }
}

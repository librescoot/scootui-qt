import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: simWindow
    title: "ScootUI Simulator"
    width: 380
    height: 800
    visible: true
    color: "#1e1e1e"

    // Position to the right of the main dashboard window
    x: 520
    y: 50

    ScrollView {
        anchors.fill: parent
        anchors.margins: 8
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 8

            // ---- Screen ----
            SectionHeader { text: "Screen" }

            RowLayout {
                Layout.fillWidth: true
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

            // ---- Presets ----
            SectionHeader { text: "Presets" }

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

            // ---- Routes ----
            SectionHeader { text: "Routes (Polyline Test)" }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 4
                rowSpacing: 4
                SimButton {
                    text: "Load Route 1"
                    Layout.fillWidth: true
                    onClicked: simulator.loadTestRoute(1)
                }
                SimButton {
                    text: "Load Route 2"
                    Layout.fillWidth: true
                    onClicked: simulator.loadTestRoute(2)
                }
                SimButton {
                    text: "Clear Route"
                    Layout.fillWidth: true
                    color: "#f44336"
                    onClicked: {
                        if (typeof navigationService !== "undefined") {
                            navigationService.clearNavigation()
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
                    onActivated: simulator.setVehicleState(currentText)

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
                    onToggled: simulator.setKickstand(checked ? "down" : "up")
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

            // ---- Controls ----
            SectionHeader { text: "Controls" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "L-Brake" }
                SimButton { text: "Tap"; small: true; onClicked: simulator.simulateBrakeTap("left") }
                SimButton { text: "Hold (3s)"; small: true; onClicked: simulator.simulateBrakeHold("left", 3000) }
                SimButton { text: "Dbl-Tap"; small: true; onClicked: simulator.simulateBrakeDoubleTap("left") }
                SimButton {
                    text: "Toggle"; small: true; checkable: true
                    color: checked ? "#f44336" : "#555"
                    onToggled: simulator.setBrakeLeft(checked)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "R-Brake" }
                SimButton { text: "Tap"; small: true; onClicked: simulator.simulateBrakeTap("right") }
                SimButton { text: "Hold (3s)"; small: true; onClicked: simulator.simulateBrakeHold("right", 3000) }
                SimButton { text: "Dbl-Tap"; small: true; onClicked: simulator.simulateBrakeDoubleTap("right") }
                SimButton {
                    text: "Toggle"; small: true; checkable: true
                    color: checked ? "#f44336" : "#555"
                    onToggled: simulator.setBrakeRight(checked)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Seatbox" }
                SimButton { text: "Tap"; small: true; onClicked: simulator.simulateSeatboxTap() }
                SimButton { text: "Hold (2s)"; small: true; onClicked: simulator.simulateSeatboxHold(2000) }
                SimButton { text: "Dbl-Tap"; small: true; onClicked: simulator.simulateSeatboxDoubleTap() }
                SimButton {
                    text: "Horn"; small: true; color: "#ff9800"
                    onPressed: simulator.setHornButton(true)
                    onReleased: simulator.setHornButton(false)
                }
            }

            RowLayout {
                Layout.fillWidth: true
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

            // ---- Engine ----
            SectionHeader { text: "Engine" }

            SimSliderRow {
                label: "Speed"
                from: 0; to: 55; value: 0; unit: "km/h"; decimals: 0
                onMoved: function(v) { simulator.setSpeed(v); simulator.setGpsSpeed(v) }
            }

            SimSliderRow {
                label: "Temp"
                from: -10; to: 120; value: 25; unit: "\u00B0C"; decimals: 0
                onMoved: function(v) { simulator.setEngineTemperature(v) }
            }

            SimSliderRow {
                label: "Odometer"
                from: 0; to: 99999; value: 1234; unit: "km"; decimals: 1
                onMoved: function(v) { simulator.setOdometer(v) }
            }

            // ---- Auto-drive ----
            SectionHeader { text: "Auto-Drive" }

            RowLayout {
                Layout.fillWidth: true
                SimButton {
                    text: simulator.autoDriveActive ? "Stop" : "Start"
                    color: simulator.autoDriveActive ? "#f44336" : "#4caf50"
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
                    color: "#ccc"
                    font.pixelSize: 11
                    Layout.preferredWidth: 48
                }
            }

            Text {
                visible: simulator.autoDriveActive
                text: "Driving at " + simulator.autoDriveSpeed.toFixed(1) + " km/h"
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
                    onToggled: simulator.setBatteryPresent(0, checked)
                }
                SimLabel { text: "State" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["active", "idle", "asleep", "unknown"]
                    currentIndex: 0
                    onActivated: simulator.setBatteryState(0, currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
            }

            SimSliderRow {
                label: "Charge"
                from: 0; to: 100; value: 80; unit: "%"; decimals: 0
                onMoved: function(v) { simulator.setBatteryCharge(0, Math.round(v)) }
            }

            SimSliderRow {
                label: "Temp"
                from: -20; to: 60; value: 25; unit: "\u00B0C"; decimals: 0
                onMoved: function(v) { simulator.setBatteryTemperature(0, Math.round(v)) }
            }

            // ---- Battery 1 ----
            SectionHeader { text: "Battery 1" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Present" }
                Switch {
                    checked: true
                    palette.highlight: "#2196F3"
                    onToggled: simulator.setBatteryPresent(1, checked)
                }
                SimLabel { text: "State" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["active", "idle", "asleep", "unknown"]
                    currentIndex: 0
                    onActivated: simulator.setBatteryState(1, currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
            }

            SimSliderRow {
                label: "Charge"
                from: 0; to: 100; value: 75; unit: "%"; decimals: 0
                onMoved: function(v) { simulator.setBatteryCharge(1, Math.round(v)) }
            }

            // ---- GPS ----
            SectionHeader { text: "GPS" }

            RowLayout {
                Layout.fillWidth: true
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
                SimLabel { text: "Lat" }
                TextField {
                    id: latField
                    Layout.fillWidth: true
                    text: "52.520008"
                    color: "white"
                    font.pixelSize: 12
                    background: Rectangle { color: "#333"; radius: 3 }
                    onEditingFinished: simulator.setGpsPosition(
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
                    onEditingFinished: simulator.setGpsPosition(
                        parseFloat(latField.text), parseFloat(text))
                }
            }

            SimSliderRow {
                label: "Course"
                from: 0; to: 359; value: 0; unit: "\u00B0"; decimals: 0
                onMoved: function(v) { simulator.setGpsCourse(v) }
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
                    onActivated: simulator.setModemState(currentText)
                    palette.button: "#333"; palette.buttonText: "white"
                    palette.window: "#333"; palette.windowText: "white"
                }
            }

            SimSliderRow {
                label: "Signal"
                from: 0; to: 100; value: 75; unit: "%"; decimals: 0
                onMoved: function(v) { simulator.setSignalQuality(Math.round(v)) }
            }

            RowLayout {
                Layout.fillWidth: true
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
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Cloud" }
                SimButton {
                    text: "Connected"; small: true
                    onClicked: simulator.setCloudConnection("connected")
                }
                SimButton {
                    text: "Disconnected"; small: true
                    onClicked: simulator.setCloudConnection("disconnected")
                }
            }

            // ---- Bluetooth ----
            SectionHeader { text: "Bluetooth" }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Status" }
                SimButton {
                    text: "Connected"; small: true
                    onClicked: simulator.setBluetoothStatus("connected")
                }
                SimButton {
                    text: "Disconnected"; small: true
                    onClicked: simulator.setBluetoothStatus("disconnected")
                }
            }

            // ---- Display ----
            SectionHeader { text: "Display" }

            RowLayout {
                Layout.fillWidth: true
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
            }

            RowLayout {
                Layout.fillWidth: true
                SimLabel { text: "Language" }
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
        property bool small: false
        property color color: "#555"
        Layout.fillWidth: !small
        Layout.preferredWidth: small ? 48 : -1
        Layout.preferredHeight: small ? 28 : 32
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
            Layout.preferredWidth: 52
        }
    }
}

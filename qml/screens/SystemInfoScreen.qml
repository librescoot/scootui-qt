import QtQuick
import "../widgets/components"
import ScootUI 1.0

// Read-only technical summary, split into pages reached from the System > Info
// submenu. The connectivity page exists so IMEI and ICCID can be read off the
// dashboard without SSH, which is what connectivity onboarding asks people for;
// the other two answer the usual support questions about boards and packs.
Rectangle {
    id: systemInfoScreen
    color: typeof ThemeStore !== "undefined" && ThemeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof ThemeStore !== "undefined" ? ThemeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)

    readonly property bool hasNet: typeof InternetStore !== "undefined"
    readonly property bool hasModem: typeof ModemStore !== "undefined"
    readonly property bool hasBle: typeof BluetoothStore !== "undefined"
    readonly property bool hasCbb: typeof CbBatteryStore !== "undefined"
    readonly property bool hasAux: typeof AuxBatteryStore !== "undefined"

    // Page indices mirror ScreenStore::SystemInfoPage.
    readonly property int pageDevice: 0
    readonly property int pageConnectivity: 1
    readonly property int pageBatteries: 2
    readonly property int page: typeof ScreenStore !== "undefined" ? ScreenStore.systemInfoPage : 0

    readonly property string placeholder: "-"

    function shown(value) {
        if (value === undefined || value === null || value === "")
            return systemInfoScreen.placeholder
        return String(value)
    }

    // Drops rows the vehicle has no value for. Used everywhere except the
    // identity block, where an explicit "-" tells the reader the SIM could not
    // be read rather than that the field does not exist.
    function present(rows) {
        return rows.filter(function (r) {
            return r.value !== "" && r.value !== systemInfoScreen.placeholder
        })
    }

    // Row labels come from the translation table. deviceRows arrive from C++
    // carrying a key rather than a label for the same reason.
    //
    // Looking a string up by key defeats QML's dependency tracking, so every
    // binding built from t() also reads `lang` to re-evaluate on a language
    // change. Dropping that read makes labels stick in the old language.
    readonly property string lang: typeof Translations !== "undefined"
                                   ? Translations.language : "en"

    function t(key, fallback) {
        return typeof Translations !== "undefined" && Translations[key] !== undefined
               ? Translations[key] : fallback
    }

    function mvToV(mv) {
        return mv > 0 ? (mv / 1000).toFixed(2) + " V" : ""
    }

    // ---- Device page ----

    readonly property var versionRows: typeof SystemInfoService !== "undefined"
                                       ? SystemInfoService.versionRows : []
    readonly property var deviceRows: {
        void systemInfoScreen.lang
        var src = typeof SystemInfoService !== "undefined" ? SystemInfoService.deviceRows : []
        return src.map(function (r) {
            return { label: systemInfoScreen.t(r.key, r.key), value: r.value }
        })
    }

    // ---- Connectivity page ----

    readonly property var identityRows: [
        { label: "IMEI", value: hasNet ? shown(InternetStore.simImei) : placeholder },
        { label: "ICCID", value: hasNet ? shown(InternetStore.simIccid) : placeholder },
        { label: "IMSI", value: hasNet ? shown(InternetStore.simImsi) : placeholder }
    ]

    readonly property var networkRows: (void systemInfoScreen.lang, present([
        { label: t("infoOperator", "Operator"), value: hasModem && ModemStore.operatorName !== ""
            ? ModemStore.operatorName
              + (ModemStore.operatorCode !== "" ? " (" + ModemStore.operatorCode + ")" : "")
            : "" },
        { label: t("infoAccessTech", "Access tech"), value: hasNet ? InternetStore.accessTech : "" },
        { label: t("infoSignal", "Signal"), value: hasNet && InternetStore.signalQuality > 0
            ? InternetStore.signalQuality + "%" : "" },
        { label: t("infoRegistration", "Registration"), value: hasModem && ModemStore.registration !== ""
            ? ModemStore.registration
              + (ModemStore.isRoaming ? " (" + t("infoRoaming", "roaming") + ")" : "")
            : "" },
        { label: t("infoSim", "SIM"), value: hasModem ? ModemStore.simState : "" },
        { label: t("infoConnectivity", "Connectivity"), value: hasNet ? InternetStore.connectivity : "" },
        { label: t("infoIpAddress", "IP address"), value: hasNet ? InternetStore.ipAddress : "" }
    ]))

    // ConnectionStatus enum: 0 = Connected, 1 = Disconnected (models/Enums.h).
    readonly property var bluetoothRows: (void systemInfoScreen.lang, present([
        { label: t("infoMac", "MAC"), value: hasBle ? BluetoothStore.macAddress : "" },
        { label: t("infoStatus", "Status"), value: hasBle
            ? (BluetoothStore.status === 0 ? t("infoConnected", "Connected")
                                           : t("infoDisconnected", "Disconnected"))
            : "" }
    ]))

    // ---- Battery pages ----

    function packRows(store) {
        void systemInfoScreen.lang
        if (typeof store === "undefined" || !store.present)
            return []
        return present([
            { label: t("infoSerial", "Serial"), value: store.serialNumber },
            { label: t("infoHealth", "Health"), value: store.stateOfHealth > 0 ? store.stateOfHealth + "%" : "" },
            { label: t("infoCycles", "Cycles"), value: store.cycleCount > 0 ? String(store.cycleCount) : "" },
            { label: t("infoCharge", "Charge"), value: store.charge > 0 ? store.charge + "%" : "" },
            { label: t("infoVoltage", "Voltage"), value: mvToV(store.voltage) },
            { label: t("infoFirmware", "Firmware"), value: store.firmwareVersion },
            { label: t("infoManufactured", "Manufactured"), value: store.manufacturingDate }
        ])
    }

    readonly property var battery0Rows: (void systemInfoScreen.lang, packRows(Batteries.slot0))
    readonly property var battery1Rows: (void systemInfoScreen.lang, packRows(Batteries.slot1))

    readonly property var cbbRows: hasCbb && CbBatteryStore.present
        ? (void systemInfoScreen.lang, present([
        { label: t("infoSerial", "Serial"), value: CbBatteryStore.serialNumber },
        { label: t("infoUniqueId", "Unique ID"), value: CbBatteryStore.uniqueId },
        { label: t("infoPartNumber", "Part number"), value: CbBatteryStore.partNumber },
        { label: t("infoHealth", "Health"), value: CbBatteryStore.stateOfHealth > 0
            ? CbBatteryStore.stateOfHealth + "%" : "" },
        { label: t("infoCycles", "Cycles"), value: CbBatteryStore.cycleCount > 0
            ? String(CbBatteryStore.cycleCount) : "" },
        { label: t("infoCharge", "Charge"), value: CbBatteryStore.chargeValid
            ? CbBatteryStore.charge + "%" : "" }
    ])) : []

    // The AUX pack has no fuel gauge, so charge is a 5-bucket estimate derived
    // from the same ADC reading as the voltage. Label it as such.
    readonly property var auxRows: (void systemInfoScreen.lang, present([
        { label: t("infoVoltage", "Voltage"), value: hasAux && AuxBatteryStore.voltageValid
            ? mvToV(AuxBatteryStore.voltage) : "" },
        { label: t("infoChargeEstimated", "Charge (est.)"), value: hasAux && AuxBatteryStore.chargeValid
            ? AuxBatteryStore.charge + "%" : "" }
    ]))

    readonly property bool pageIsEmpty: {
        if (page === pageConnectivity)
            return identityRows.length === 0 && networkRows.length === 0
                   && bluetoothRows.length === 0
        if (page === pageBatteries)
            return battery0Rows.length === 0 && battery1Rows.length === 0
                   && cbbRows.length === 0 && auxRows.length === 0
        return versionRows.length === 0 && deviceRows.length === 0
    }

    readonly property string pageTitle: {
        if (typeof Translations === "undefined")
            return "System Info"
        if (page === pageConnectivity) return Translations.menuInfoConnectivity
        if (page === pageBatteries) return Translations.menuInfoBatteries
        return Translations.menuInfoComponents
    }

    Component.onCompleted: {
        if (typeof SystemInfoService !== "undefined")
            SystemInfoService.loadVersions()
    }

    function closeScreen() {
        if (typeof ScreenStore !== "undefined")
            ScreenStore.closeSystemInfo()
    }

    // Reset the scroll position when switching pages, otherwise a short page
    // opens already scrolled past its own content.
    onPageChanged: flickable.contentY = 0

    Connections {
        target: typeof InputHandler !== "undefined" ? InputHandler : null
        function onLeftTap() {
            var maxY = Math.max(0, flickable.contentHeight - flickable.height)
            scrollAnim.to = Math.min(flickable.contentY + 120, maxY)
            scrollAnim.restart()
        }
        function onLeftHold() {
            scrollAnim.to = Math.max(flickable.contentY - 120, 0)
            scrollAnim.restart()
        }
        function onRightTap() { systemInfoScreen.closeScreen() }
    }

    Column {
        anchors.fill: parent

        // Header
        Item {
            id: header
            width: parent.width
            height: 56

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: systemInfoScreen.pageTitle
                color: systemInfoScreen.textPrimary
                font.pixelSize: ThemeStore.fontBody + 6
                font.weight: Font.DemiBold
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: systemInfoScreen.dividerColor
            }
        }

        Flickable {
            id: flickable
            width: parent.width
            height: parent.height - header.height - controlBar.height
            contentHeight: content.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            NumberAnimation {
                id: scrollAnim
                target: flickable
                property: "contentY"
                duration: 200
                easing.type: Easing.OutCubic
            }

            Column {
                id: content
                width: flickable.width
                spacing: 0

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageDevice
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoFirmware : "FIRMWARE"
                    rows: systemInfoScreen.versionRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageDevice
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoBoards : "BOARDS"
                    rows: systemInfoScreen.deviceRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageConnectivity
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoBluetooth : "BLUETOOTH"
                    rows: systemInfoScreen.bluetoothRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageConnectivity
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoIdentity : "SIM / MODEM IDENTITY"
                    rows: systemInfoScreen.identityRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageConnectivity
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoNetwork : "NETWORK"
                    rows: systemInfoScreen.networkRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageBatteries
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoBattery0 : "BATTERY 1"
                    rows: systemInfoScreen.battery0Rows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageBatteries
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoBattery1 : "BATTERY 2"
                    rows: systemInfoScreen.battery1Rows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageBatteries
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoCbb : "CONNECTIVITY BATTERY"
                    rows: systemInfoScreen.cbbRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageBatteries
                    sectionTitle: typeof Translations !== "undefined"
                                  ? Translations.systemInfoAux : "AUX BATTERY"
                    rows: systemInfoScreen.auxRows
                }

                // Shown when a whole page has nothing to report, so it never
                // renders as a blank screen the rider cannot interpret.
                Item {
                    width: content.width
                    height: 160
                    visible: systemInfoScreen.pageIsEmpty

                    Text {
                        anchors.centerIn: parent
                        text: typeof Translations !== "undefined"
                              ? Translations.systemInfoUnavailable : "No data reported"
                        color: systemInfoScreen.textSecondary
                        font.pixelSize: ThemeStore.fontBody
                    }
                }

                Item { width: 1; height: 24 }
            }
        }

        // Footer
        Rectangle {
            id: controlBar
            width: parent.width
            height: controlHints.height
            color: systemInfoScreen.isDark ? "black" : "white"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: systemInfoScreen.dividerColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                leftAction: typeof Translations !== "undefined"
                            ? Translations.aboutScrollAction : "Scroll"
                rightAction: typeof Translations !== "undefined"
                             ? Translations.aboutBackAction : "Back"
            }
        }
    }

    // Section header plus label/value rows. Values are monospace so long
    // digit strings stay comparable against a phone screen or a form.
    component InfoSection: Column {
        id: infoSection
        property string sectionTitle: ""
        property var rows: []
        property bool pageActive: true

        spacing: 0
        visible: pageActive && rows.length > 0

        Item {
            width: infoSection.width
            height: 34

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 6
                text: infoSection.sectionTitle
                color: systemInfoScreen.textSecondary
                font.pixelSize: ThemeStore.fontCaption
                font.weight: Font.Bold
                font.letterSpacing: 1.5
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 20
                width: parent.width - 40
                height: 1
                color: systemInfoScreen.dividerColor
            }
        }

        Repeater {
            model: infoSection.rows

            delegate: Item {
                width: infoSection.width
                height: 30

                Text {
                    id: rowLabel
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    // versionRows already carry a trailing colon; the rows
                    // built here do not.
                    text: modelData.label.slice(-1) === ":" ? modelData.label
                                                            : modelData.label + ":"
                    color: systemInfoScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                }

                Text {
                    anchors.left: rowLabel.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignRight
                    text: modelData.value
                    color: systemInfoScreen.textPrimary
                    font.pixelSize: ThemeStore.fontBody
                    font.family: "monospace"
                    elide: Text.ElideRight
                }
            }
        }
    }
}

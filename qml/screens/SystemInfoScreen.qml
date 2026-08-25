import QtQuick
import "../widgets/components"

// Read-only technical summary, split into pages reached from the System > Info
// submenu. The connectivity page exists so IMEI and ICCID can be read off the
// dashboard without SSH, which is what connectivity onboarding asks people for;
// the other two answer the usual support questions about boards and packs.
Rectangle {
    id: systemInfoScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)

    readonly property bool hasNet: typeof internetStore !== "undefined"
    readonly property bool hasModem: typeof modemStore !== "undefined"
    readonly property bool hasBle: typeof bluetoothStore !== "undefined"
    readonly property bool hasCbb: typeof cbBatteryStore !== "undefined"
    readonly property bool hasAux: typeof auxBatteryStore !== "undefined"

    // Page indices mirror ScreenStore::SystemInfoPage.
    readonly property int pageDevice: 0
    readonly property int pageConnectivity: 1
    readonly property int pageBatteries: 2
    readonly property int pageMaps: 3
    readonly property int page: typeof screenStore !== "undefined" ? screenStore.systemInfoPage : 0

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
    readonly property string lang: typeof translations !== "undefined"
                                   ? translations.language : "en"

    function t(key, fallback) {
        return typeof translations !== "undefined" && translations[key] !== undefined
               ? translations[key] : fallback
    }

    function mvToV(mv) {
        return mv > 0 ? (mv / 1000).toFixed(2) + " V" : ""
    }

    // ---- Device page ----

    readonly property var versionRows: typeof systemInfoService !== "undefined"
                                       ? systemInfoService.versionRows : []
    readonly property var deviceRows: {
        void systemInfoScreen.lang
        var src = typeof systemInfoService !== "undefined" ? systemInfoService.deviceRows : []
        return src.map(function (r) {
            return { label: systemInfoScreen.t(r.key, r.key), value: r.value }
        })
    }

    // ---- Connectivity page ----

    readonly property var identityRows: [
        { label: "IMEI", value: hasNet ? shown(internetStore.simImei) : placeholder },
        { label: "ICCID", value: hasNet ? shown(internetStore.simIccid) : placeholder },
        { label: "IMSI", value: hasNet ? shown(internetStore.simImsi) : placeholder }
    ]

    readonly property var networkRows: (void systemInfoScreen.lang, present([
        { label: t("infoOperator", "Operator"), value: hasModem && modemStore.operatorName !== ""
            ? modemStore.operatorName
              + (modemStore.operatorCode !== "" ? " (" + modemStore.operatorCode + ")" : "")
            : "" },
        { label: t("infoAccessTech", "Access tech"), value: hasNet ? internetStore.accessTech : "" },
        { label: t("infoSignal", "Signal"), value: hasNet && internetStore.signalQuality > 0
            ? internetStore.signalQuality + "%" : "" },
        { label: t("infoRegistration", "Registration"), value: hasModem && modemStore.registration !== ""
            ? modemStore.registration
              + (modemStore.isRoaming ? " (" + t("infoRoaming", "roaming") + ")" : "")
            : "" },
        { label: t("infoSim", "SIM"), value: hasModem ? modemStore.simState : "" },
        { label: t("infoConnectivity", "Connectivity"), value: hasNet ? internetStore.connectivity : "" },
        { label: t("infoIpAddress", "IP address"), value: hasNet ? internetStore.ipAddress : "" }
    ]))

    // ConnectionStatus enum: 0 = Connected, 1 = Disconnected (models/Enums.h).
    readonly property var bluetoothRows: (void systemInfoScreen.lang, present([
        { label: t("infoMac", "MAC"), value: hasBle ? bluetoothStore.macAddress : "" },
        { label: t("infoStatus", "Status"), value: hasBle
            ? (bluetoothStore.status === 0 ? t("infoConnected", "Connected")
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

    readonly property var battery0Rows: typeof battery0Store !== "undefined"
                                        ? (void systemInfoScreen.lang, packRows(battery0Store)) : []
    readonly property var battery1Rows: typeof battery1Store !== "undefined"
                                        ? (void systemInfoScreen.lang, packRows(battery1Store)) : []

    readonly property var cbbRows: hasCbb && cbBatteryStore.present
        ? (void systemInfoScreen.lang, present([
        { label: t("infoSerial", "Serial"), value: cbBatteryStore.serialNumber },
        { label: t("infoUniqueId", "Unique ID"), value: cbBatteryStore.uniqueId },
        { label: t("infoPartNumber", "Part number"), value: cbBatteryStore.partNumber },
        { label: t("infoHealth", "Health"), value: cbBatteryStore.stateOfHealth > 0
            ? cbBatteryStore.stateOfHealth + "%" : "" },
        { label: t("infoCycles", "Cycles"), value: cbBatteryStore.cycleCount > 0
            ? String(cbBatteryStore.cycleCount) : "" },
        { label: t("infoCharge", "Charge"), value: cbBatteryStore.chargeValid
            ? cbBatteryStore.charge + "%" : "" }
    ])) : []

    // The AUX pack has no fuel gauge, so charge is a 5-bucket estimate derived
    // from the same ADC reading as the voltage. Label it as such.
    readonly property var auxRows: (void systemInfoScreen.lang, present([
        { label: t("infoVoltage", "Voltage"), value: hasAux && auxBatteryStore.voltageValid
            ? mvToV(auxBatteryStore.voltage) : "" },
        { label: t("infoChargeEstimated", "Charge (est.)"), value: hasAux && auxBatteryStore.chargeValid
            ? auxBatteryStore.charge + "%" : "" }
    ]))

    // ---- Maps page ----

    // Keyed exactly as the `maps` Redis hash, so what the rider reads here and
    // what a consumer reads over Redis cannot disagree.
    readonly property var mapInfo: typeof mapDownloadService !== "undefined"
                                   ? mapDownloadService.mapInfo : ({})

    function humanBytes(bytes) {
        var n = Number(bytes)
        if (!isFinite(n) || n <= 0)
            return ""
        // Decimal MB, matching how the tile repos quote their sizes.
        if (n >= 1000000000)
            return (n / 1000000000).toFixed(1) + " GB"
        return Math.round(n / 1000000) + " MB"
    }

    // Full sha256 does not fit the 480px panel and is not read digit by digit
    // anyway. The leading 12 are plenty to tell two builds apart by eye.
    function shortDigest(digest) {
        return digest ? String(digest).substring(0, 12) : ""
    }

    // ISO-8601 down to the day. The time of day of an upstream tile build is
    // noise for someone answering "how old are my maps".
    function shortDate(iso) {
        if (!iso)
            return ""
        var t = String(iso).indexOf("T")
        return t > 0 ? String(iso).substring(0, t) : String(iso)
    }

    function yesNo(value) {
        return value === "true" ? t("infoYes", "Yes") : t("infoNo", "No")
    }

    readonly property var mapRegionRows: (void systemInfoScreen.lang, present([
        { label: t("infoRegion", "Region"),
          value: mapInfo["region-name"] || mapInfo["region"] || "" },
        { label: t("infoLastChecked", "Last checked"),
          value: shortDate(mapInfo["last-update-check"]) },
        { label: t("infoUpdateAvailable", "Update available"),
          value: mapInfo["update-available"] !== undefined
                 ? yesNo(mapInfo["update-available"]) : "" }
    ]))

    // An artifact with no entry at all is absent from disk, which is worth
    // saying outright rather than rendering as a section with no rows.
    function tileRows(prefix) {
        void systemInfoScreen.lang
        if (mapInfo[prefix + ":size"] === undefined)
            return [{ label: t("infoStatus", "Status"), value: t("infoNotInstalled", "Not installed") }]
        return present([
            { label: t("infoSize", "Size"), value: humanBytes(mapInfo[prefix + ":size"]) },
            { label: t("infoChecksum", "Checksum"), value: shortDigest(mapInfo[prefix + ":sha256"]) },
            { label: t("infoPublished", "Published"), value: shortDate(mapInfo[prefix + ":published-at"]) },
            { label: t("infoInstalled", "Installed"), value: shortDate(mapInfo[prefix + ":mtime"]) }
        ])
    }

    readonly property var mapDisplayRows: (void systemInfoScreen.lang, tileRows("map"))
    readonly property var mapRoutingRows: (void systemInfoScreen.lang, tileRows("routing"))

    readonly property bool pageIsEmpty: {
        if (page === pageConnectivity)
            return identityRows.length === 0 && networkRows.length === 0
                   && bluetoothRows.length === 0
        if (page === pageBatteries)
            return battery0Rows.length === 0 && battery1Rows.length === 0
                   && cbbRows.length === 0 && auxRows.length === 0
        // tileRows() always returns at least a "not installed" row, so the maps
        // page is never empty.
        if (page === pageMaps)
            return false
        return versionRows.length === 0 && deviceRows.length === 0
    }

    readonly property string pageTitle: {
        if (typeof translations === "undefined")
            return "System Info"
        if (page === pageConnectivity) return translations.menuInfoConnectivity
        if (page === pageBatteries) return translations.menuInfoBatteries
        if (page === pageMaps) return translations.menuInfoMaps
        return translations.menuInfoComponents
    }

    Component.onCompleted: {
        if (typeof systemInfoService !== "undefined")
            systemInfoService.loadVersions()
    }

    function closeScreen() {
        if (typeof screenStore !== "undefined")
            screenStore.closeSystemInfo()
        if (typeof menuStore !== "undefined")
            menuStore.resume()
    }

    // Reset the scroll position when switching pages, otherwise a short page
    // opens already scrolled past its own content.
    onPageChanged: flickable.contentY = 0

    readonly property bool canScrollDown: flickable.contentHeight > flickable.height
                                           && flickable.contentY + flickable.height < flickable.contentHeight - 2
    readonly property bool canScrollUp: flickable.contentY > 2

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            var maxY = Math.max(0, flickable.contentHeight - flickable.height)
            scrollAnim.to = Math.min(flickable.contentY + 120, maxY)
            scrollAnim.restart()
        }
        function onLeftHold() { systemInfoScreen.closeScreen() }
        function onRightHold() {
            scrollAnim.to = Math.max(flickable.contentY - 120, 0)
            scrollAnim.restart()
        }
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
                font.pixelSize: themeStore.fontBody + 6
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
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoFirmware : "FIRMWARE"
                    rows: systemInfoScreen.versionRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageDevice
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoBoards : "BOARDS"
                    rows: systemInfoScreen.deviceRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageConnectivity
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoBluetooth : "BLUETOOTH"
                    rows: systemInfoScreen.bluetoothRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageConnectivity
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoIdentity : "SIM / MODEM IDENTITY"
                    rows: systemInfoScreen.identityRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageConnectivity
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoNetwork : "NETWORK"
                    rows: systemInfoScreen.networkRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageBatteries
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoBattery0 : "BATTERY 1"
                    rows: systemInfoScreen.battery0Rows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageBatteries
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoBattery1 : "BATTERY 2"
                    rows: systemInfoScreen.battery1Rows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageBatteries
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoCbb : "CONNECTIVITY BATTERY"
                    rows: systemInfoScreen.cbbRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageBatteries
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoAux : "AUX BATTERY"
                    rows: systemInfoScreen.auxRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageMaps
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoMapsRegion : "REGION"
                    rows: systemInfoScreen.mapRegionRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageMaps
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoMapsDisplay : "DISPLAY TILES"
                    rows: systemInfoScreen.mapDisplayRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageMaps
                    sectionTitle: typeof translations !== "undefined"
                                  ? translations.systemInfoMapsRouting : "ROUTING TILES"
                    rows: systemInfoScreen.mapRoutingRows
                }

                // Shown when a whole page has nothing to report, so it never
                // renders as a blank screen the rider cannot interpret.
                Item {
                    width: content.width
                    height: 160
                    visible: systemInfoScreen.pageIsEmpty

                    Text {
                        anchors.centerIn: parent
                        text: typeof translations !== "undefined"
                              ? translations.systemInfoUnavailable : "No data reported"
                        color: systemInfoScreen.textSecondary
                        font.pixelSize: themeStore.fontBody
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
                // The labels come and go with the scroll position; pin the row
                // count so the content above does not shift while scrolling,
                // and so the bar's height cannot feed back into the flickable
                // it is sized against.
                reservedRows: 2
                leftTap: systemInfoScreen.canScrollDown
                    ? (typeof translations !== "undefined" ? translations.controlScroll : "Scroll")
                    : ""
                leftHold: typeof translations !== "undefined"
                          ? translations.controlBack : "Back"
                rightHold: systemInfoScreen.canScrollUp
                    ? (typeof translations !== "undefined" ? translations.controlScrollUp : "Scroll up")
                    : ""
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
                font.pixelSize: themeStore.fontCaption
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
                    font.pixelSize: themeStore.fontBody
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
                    font.pixelSize: themeStore.fontBody
                    font.family: "monospace"
                    elide: Text.ElideRight
                }
            }
        }
    }
}

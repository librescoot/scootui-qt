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

    // Row labels come from the translation table. Board rows arrive from C++
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

    function uVToV(uV) {
        return uV > 0 ? (uV / 1000000).toFixed(2) + " V" : ""
    }

    // The main BMS reports capacity in mAh, the connectivity battery's gauge
    // in µAh; both render as Ah.
    function mahToAh(mah) {
        return (mah / 1000).toFixed(1) + " Ah"
    }

    function uahToAh(uah) {
        return (uah / 1000000).toFixed(1) + " Ah"
    }

    // ---- Device page ----

    function keyedRows(src) {
        void systemInfoScreen.lang
        return (src || []).map(function (r) {
            return { label: systemInfoScreen.t(r.key, r.key), value: r.value }
        })
    }

    readonly property var mdbBoardRows: keyedRows(typeof systemInfoService !== "undefined"
                                                  ? systemInfoService.mdbBoardRows : null)
    readonly property var dbcBoardRows: keyedRows(typeof systemInfoService !== "undefined"
                                                  ? systemInfoService.dbcBoardRows : null)
    readonly property var nrfBoardRows: keyedRows(typeof systemInfoService !== "undefined"
                                                  ? systemInfoService.nrfBoardRows : null)
    readonly property var ecuBoardRows: keyedRows(typeof systemInfoService !== "undefined"
                                                  ? systemInfoService.ecuBoardRows : null)

    // ---- Connectivity page ----

    readonly property var identityRows: [
        { label: "IMEI", value: hasNet ? shown(internetStore.simImei) : placeholder },
        { label: "ICCID", value: hasNet ? shown(internetStore.simIccid) : placeholder },
        { label: "IMSI", value: hasNet ? shown(internetStore.simImsi) : placeholder }
    ]

    // Ordered along the data path: SIM state, registration, then the radio
    // facts behind them, then what the connection delivers.
    readonly property var networkRows: (void systemInfoScreen.lang, present([
        { label: t("infoSim", "SIM"), value: hasModem ? modemStore.simState : "" },
        { label: t("infoRegistration", "Registration"), value: hasModem && modemStore.registration !== ""
            ? modemStore.registration
              + (modemStore.isRoaming ? " (" + t("infoRoaming", "roaming") + ")" : "")
            : "" },
        { label: t("infoOperator", "Operator"), value: hasModem && modemStore.operatorName !== ""
            ? modemStore.operatorName
              + (modemStore.operatorCode !== "" ? " (" + modemStore.operatorCode + ")" : "")
            : "" },
        { label: t("infoAccessTech", "Access tech"), value: hasNet ? internetStore.accessTech : "" },
        { label: t("infoSignal", "Signal"), value: hasNet && internetStore.signalQuality > 0
            ? internetStore.signalQuality + "%" : "" },
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

    // Row values are either a plain string or a segment list; segments let an
    // icon glyph sit inside a value (a cycle arrow between the two health
    // numbers, a low-battery alert after the charge).
    function seg(text, icon, warning) {
        var s = { text: text }
        if (icon) s.icon = true
        if (warning) s.warning = true
        return s
    }

    // Remaining/full Ah with the charge percentage rolled in; falls back to a
    // plain charge row when the pack never reported a capacity. Low-soc shows
    // as the battery-alert icon, not a row.
    function capacityRow(label, remaining, full, charge, toAh, lowSoc) {
        var segments = []
        if (full > 0) {
            segments.push(seg(toAh(remaining) + " / " + toAh(full) + " (" + charge + "%)"))
        } else if (charge > 0) {
            label = t("infoCharge", "Charge")
            segments.push(seg(charge + "%"))
        } else {
            return null
        }
        if (lowSoc)
            segments.push(seg(MaterialIcon.iconBatteryAlert, true, true))
        return { label: label, valueSegments: segments }
    }

    // Cycles and health share a row: "94 ⟳ / 91%".
    function healthRow(cycles, soh) {
        var segments = []
        if (cycles > 0)
            segments.push(seg(String(cycles)), seg(MaterialIcon.iconAutorenew, true))
        if (soh > 0)
            segments.push(seg((cycles > 0 ? " / " : "") + soh + "%"))
        return segments.length
            ? { label: t("infoHealth", "Health"), valueSegments: segments } : null
    }

    function packRows(store) {
        void systemInfoScreen.lang
        if (typeof store === "undefined" || !store.present)
            return []
        var rows = [
            capacityRow(t("infoCapacity", "Capacity"), store.remainingCapacity,
                        store.fullCapacity, store.charge, mahToAh, store.lowSoc),
            { label: t("infoVoltage", "Voltage"), value: mvToV(store.voltage) },
            healthRow(store.cycleCount, store.stateOfHealth),
            { label: t("infoSerial", "Serial"), value: store.serialNumber },
            { label: t("infoManufactured", "Manufactured"), value: store.manufacturingDate },
            { label: t("infoFirmware", "Firmware"), value: store.firmwareVersion }
        ].filter(function (r) { return r !== null })
        return present(rows)
    }

    readonly property var battery0Rows: typeof battery0Store !== "undefined"
                                        ? (void systemInfoScreen.lang, packRows(battery0Store)) : []
    readonly property var battery1Rows: typeof battery1Store !== "undefined"
                                        ? (void systemInfoScreen.lang, packRows(battery1Store)) : []

    readonly property var cbbRows: hasCbb && cbBatteryStore.present
        ? (void systemInfoScreen.lang, present([
        capacityRow(t("infoCapacity", "Capacity"), cbBatteryStore.remainingCapacity,
                    cbBatteryStore.fullCapacity, cbBatteryStore.charge, uahToAh, false),
        { label: t("infoCellVoltage", "Cell voltage"),
          value: cbBatteryStore.cellVoltage > 0 ? uVToV(cbBatteryStore.cellVoltage) : "" },
        healthRow(cbBatteryStore.cycleCount, cbBatteryStore.stateOfHealth),
        { label: t("infoSerial", "Serial"), value: cbBatteryStore.serialNumber },
        { label: t("infoUniqueId", "Unique ID"), value: cbBatteryStore.uniqueId },
        { label: t("infoPartNumber", "Part number"), value: cbBatteryStore.partNumber }
    ].filter(function (r) { return r !== null }))) : []

    // The AUX pack has no fuel gauge, so charge is a 5-bucket estimate derived
    // from the same ADC reading as the voltage. Label it as such.
    readonly property var auxRows: (void systemInfoScreen.lang, present([
        { label: t("infoChargeEstimated", "Charge (est.)"), value: hasAux && auxBatteryStore.chargeValid
            ? auxBatteryStore.charge + "%" : "" },
        { label: t("infoVoltage", "Voltage"), value: hasAux && auxBatteryStore.voltageValid
            ? mvToV(auxBatteryStore.voltage) : "" }
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

    readonly property var mapRegionRows: (void systemInfoScreen.lang, present([
        { label: t("infoRegion", "Region"),
          value: mapInfo["region-name"] || mapInfo["region"] || "" },
        { label: t("infoLastChecked", "Last checked"),
          value: shortDate(mapInfo["last-update-check"])
                 + (mapInfo["update-available"] === "true"
                    ? " · " + t("infoUpdateAvailable", "update available") : "") }
    ]))

    // An artifact with no entry at all is absent from disk, which is worth
    // saying outright rather than rendering as a section with no rows.
    // Freshness first; the checksum goes last as the longest value.
    function tileRows(prefix) {
        void systemInfoScreen.lang
        if (mapInfo[prefix + ":size"] === undefined)
            return [{ label: t("infoStatus", "Status"), value: t("infoNotInstalled", "Not installed") }]
        return present([
            { label: t("infoPublished", "Published"), value: shortDate(mapInfo[prefix + ":published-at"]) },
            { label: t("infoInstalled", "Installed"), value: shortDate(mapInfo[prefix + ":mtime"]) },
            { label: t("infoSize", "Size"), value: humanBytes(mapInfo[prefix + ":size"]) },
            { label: t("infoChecksum", "Checksum"), value: shortDigest(mapInfo[prefix + ":sha256"]) }
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
        return mdbBoardRows.length === 0 && dbcBoardRows.length === 0
               && nrfBoardRows.length === 0 && ecuBoardRows.length === 0
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

                // One block per board; empty fields are dropped by the service.
                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageDevice
                    sectionTitle: "MDB"
                    rows: systemInfoScreen.mdbBoardRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageDevice
                    sectionTitle: "DBC"
                    rows: systemInfoScreen.dbcBoardRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageDevice
                    sectionTitle: "nRF"
                    rows: systemInfoScreen.nrfBoardRows
                }

                InfoSection {
                    width: content.width
                    pageActive: systemInfoScreen.page === systemInfoScreen.pageDevice
                    sectionTitle: "ECU"
                    rows: systemInfoScreen.ecuBoardRows
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

                // Plain string value.
                Text {
                    visible: modelData.valueSegments === undefined
                    anchors.left: rowLabel.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignRight
                    text: modelData.value || ""
                    color: systemInfoScreen.textPrimary
                    font.pixelSize: themeStore.fontBody
                    font.family: "monospace"
                    elide: Text.ElideRight
                }

                // Segment value: {text, icon, warning} runs laid out left to
                // right, right-aligned as a whole. Icons render in the Material
                // Icons face; warning segments take the warning color.
                Row {
                    visible: modelData.valueSegments !== undefined
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3

                    Repeater {
                        model: modelData.valueSegments || []

                        delegate: Text {
                            required property var modelData
                            text: modelData.text
                            color: modelData.warning ? themeStore.statusWarning
                                                     : systemInfoScreen.textPrimary
                            font.pixelSize: themeStore.fontBody
                            font.family: modelData.icon ? "Material Icons" : "monospace"
                        }
                    }
                }
            }
        }
    }
}

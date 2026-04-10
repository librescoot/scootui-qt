import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/components"

Rectangle {
    id: addressScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color textTertiary: isDark ? "#4DFFFFFF" : "#1F000000"
    readonly property color surfaceColor: isDark ? "#1E1E1E" : "#F5F5F5"
    readonly property color selectedBg: isDark ? "#3DFFFFFF" : "#1F000000"
    readonly property color borderColor: isDark ? "#1AFFFFFF" : "#1F000000"
    readonly property color errorColor: "#EF5350"
    readonly property color goColor: "#4CAF50"

    // Address database status constants (match C++ enum)
    readonly property int statusIdle: 0
    readonly property int statusLoading: 1
    readonly property int statusBuilding: 2
    readonly property int statusReady: 3
    readonly property int statusError: 4

    readonly property int dbStatus: typeof addressDatabase !== "undefined" ? addressDatabase.status : statusError

    // Phase constants
    readonly property int phaseLoading: 0
    readonly property int phaseCityLetters: 1
    readonly property int phaseCityList: 2
    readonly property int phaseStreetLetters: 3
    readonly property int phaseStreetList: 4
    readonly property int phaseHouseNumbers: 5
    readonly property int phaseConfirm: 6

    // Auto-transition threshold
    readonly property int maxListItems: 8

    // State
    property int phase: phaseLoading
    property string cityPrefix: ""
    property string streetPrefix: ""
    property var validChars: []
    property int charIndex: 0
    property var itemList: []
    property int listIndex: 0
    property string selectedCity: ""
    property string selectedStreet: ""
    property string selectedPostcode: ""
    property string selectedHouse: ""
    property double destLat: 0
    property double destLng: 0
    property bool loadingHouseNumbers: false

    // When database becomes ready, start city letter input
    onDbStatusChanged: {
        if (dbStatus === statusReady) {
            enterCityLetters("")
        }
    }

    Component.onCompleted: {
        if (dbStatus === statusReady) {
            enterCityLetters("")
        } else if (dbStatus === statusIdle || dbStatus === statusError) {
            if (typeof addressDatabase !== "undefined")
                addressDatabase.initialize()
        }
    }

    // --- Phase transition functions ---

    function enterCityLetters(prefix) {
        phase = phaseCityLetters
        cityPrefix = prefix
        charIndex = 0
        refreshValidChars()
    }

    function enterCityList(autoSelect) {
        phase = phaseCityList
        listIndex = 0
        itemList = addressDatabase.getMatchingCities(cityPrefix)
        if (autoSelect !== false && itemList.length === 1) {
            selectCity(0)
        }
    }

    function enterStreetLetters(prefix) {
        phase = phaseStreetLetters
        streetPrefix = prefix
        charIndex = 0
        refreshValidChars()
    }

    function enterStreetList(autoSelect) {
        phase = phaseStreetList
        listIndex = 0
        itemList = addressDatabase.getMatchingStreets(selectedCity, streetPrefix)
        if (autoSelect !== false && itemList.length === 1) {
            selectStreet(0)
        }
    }

    function enterHouseNumbers() {
        loadingHouseNumbers = true
        addressDatabase.queryHouseNumbers(selectedCity, selectedStreet, selectedPostcode)
    }

    Connections {
        target: typeof addressDatabase !== "undefined" ? addressDatabase : null

        function onHouseNumbersReady(houses) {
            addressScreen.loadingHouseNumbers = false
            if (houses.length <= 1) {
                if (houses.length === 1) {
                    addressScreen.selectedHouse = houses[0].housenumber
                    addressScreen.destLat = houses[0].latitude
                    addressScreen.destLng = houses[0].longitude
                } else {
                    addressScreen.selectedHouse = ""
                    var coords = addressDatabase.getStreetCoordinates(
                        addressScreen.selectedCity, addressScreen.selectedStreet)
                    addressScreen.destLat = coords.latitude || 0
                    addressScreen.destLng = coords.longitude || 0
                }
                addressScreen.phase = addressScreen.phaseConfirm
                return
            }
            addressScreen.phase = addressScreen.phaseHouseNumbers
            addressScreen.listIndex = 0
            addressScreen.itemList = houses
        }
    }

    function enterConfirm() {
        phase = phaseConfirm
    }

    // --- Char carousel logic ---

    function refreshValidChars() {
        if (phase === phaseCityLetters) {
            validChars = addressDatabase.getValidCityChars(cityPrefix)
        } else if (phase === phaseStreetLetters) {
            validChars = addressDatabase.getValidStreetChars(selectedCity, streetPrefix)
        }
        charIndex = 0
    }

    function cycleChar() {
        if (validChars.length === 0) return
        charIndex = (charIndex + 1) % validChars.length
    }

    function selectCurrentChar() {
        if (validChars.length === 0) return

        var ch = validChars[charIndex]
        if (phase === phaseCityLetters) {
            cityPrefix += ch
            var cityCount = addressDatabase.getCityCount(cityPrefix)
            if (cityCount <= maxListItems && cityCount > 0) {
                enterCityList()
            } else if (cityCount === 0) {
                cityPrefix = cityPrefix.slice(0, -1)
            } else {
                refreshValidChars()
            }
        } else if (phase === phaseStreetLetters) {
            streetPrefix += ch
            var streetCount = addressDatabase.getStreetCount(selectedCity, streetPrefix)
            if (streetCount <= maxListItems && streetCount > 0) {
                enterStreetList()
            } else if (streetCount === 0) {
                streetPrefix = streetPrefix.slice(0, -1)
            } else {
                refreshValidChars()
            }
        }
    }

    function backspace() {
        if (phase === phaseCityLetters) {
            if (cityPrefix.length > 0) {
                cityPrefix = cityPrefix.slice(0, -1)
                refreshValidChars()
            } else {
                if (typeof screenStore !== "undefined")
                    screenStore.setScreen(1)
            }
        } else if (phase === phaseStreetLetters) {
            if (streetPrefix.length > 0) {
                streetPrefix = streetPrefix.slice(0, -1)
                refreshValidChars()
            } else {
                enterCityList(false)
            }
        }
    }

    // --- List selection logic ---

    function cycleListItem() {
        if (itemList.length === 0) return
        listIndex = (listIndex + 1) % itemList.length
    }

    function selectCity(index) {
        if (index === undefined) index = listIndex
        if (index < 0 || index >= itemList.length) return
        selectedCity = itemList[index]
        streetPrefix = ""
        enterStreetLetters("")
    }

    function selectStreet(index) {
        if (index === undefined) index = listIndex
        if (index < 0 || index >= itemList.length) return
        var entry = itemList[index]
        selectedStreet = entry.street
        selectedPostcode = entry.postcode || ""
        enterHouseNumbers()
    }

    function selectHouseNumber() {
        if (listIndex < 0 || listIndex >= itemList.length) return
        var entry = itemList[listIndex]
        selectedHouse = entry.housenumber
        destLat = entry.latitude
        destLng = entry.longitude
        enterConfirm()
    }

    function confirmAndNavigate() {
        var addressLabel = selectedStreet
        if (selectedHouse !== "")
            addressLabel += " " + selectedHouse
        addressLabel += ", " + selectedCity

        if (typeof navigationService !== "undefined") {
            navigationService.setDestination(destLat, destLng, addressLabel)
        }
        if (typeof screenStore !== "undefined") {
            screenStore.setScreen(1)
        }
    }

    function matchCountText() {
        var tr = typeof translations !== "undefined" ? translations : null
        if (phase === phaseCityLetters) {
            var count = addressDatabase.getCityCount(cityPrefix)
            var label = tr ? tr.navCities : "cities"
            return count + " " + label
        } else if (phase === phaseStreetLetters) {
            var scount = addressDatabase.getStreetCount(selectedCity, streetPrefix)
            var slabel = tr ? tr.navStreets : "streets"
            return scount + " " + slabel
        }
        return ""
    }

    // --- Input handling ---

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null

        function onLeftTap() {
            if (addressScreen.loadingHouseNumbers) return
            if (addressScreen.phase === addressScreen.phaseCityLetters ||
                addressScreen.phase === addressScreen.phaseStreetLetters) {
                addressScreen.cycleChar()
            } else if (addressScreen.phase === addressScreen.phaseCityList ||
                       addressScreen.phase === addressScreen.phaseStreetList ||
                       addressScreen.phase === addressScreen.phaseHouseNumbers) {
                addressScreen.cycleListItem()
            }
        }

        function onLeftHold() {
            if (addressScreen.loadingHouseNumbers) return
            switch (addressScreen.phase) {
            case addressScreen.phaseCityLetters:
                addressScreen.backspace()
                break
            case addressScreen.phaseCityList:
                addressScreen.enterCityLetters(addressScreen.cityPrefix)
                break
            case addressScreen.phaseStreetLetters:
                addressScreen.backspace()
                break
            case addressScreen.phaseStreetList:
                addressScreen.enterStreetLetters(addressScreen.streetPrefix)
                break
            case addressScreen.phaseHouseNumbers:
                addressScreen.enterStreetList(false)
                break
            case addressScreen.phaseConfirm:
                addressScreen.enterStreetList(false)
                break
            }
        }

        function onRightTap() {
            if (addressScreen.loadingHouseNumbers) return
            if (addressScreen.dbStatus === addressScreen.statusBuilding) {
                if (typeof addressDatabase !== "undefined")
                    addressDatabase.cancelBuild()
                return
            }
            if (addressScreen.dbStatus !== addressScreen.statusReady) {
                if (typeof screenStore !== "undefined")
                    screenStore.setScreen(1)
                return
            }

            switch (addressScreen.phase) {
            case addressScreen.phaseCityLetters:
                addressScreen.selectCurrentChar()
                break
            case addressScreen.phaseCityList:
                addressScreen.selectCity()
                break
            case addressScreen.phaseStreetLetters:
                addressScreen.selectCurrentChar()
                break
            case addressScreen.phaseStreetList:
                addressScreen.selectStreet()
                break
            case addressScreen.phaseHouseNumbers:
                addressScreen.selectHouseNumber()
                break
            case addressScreen.phaseConfirm:
                addressScreen.confirmAndNavigate()
                break
            }
        }
    }

    // =====================================================================
    // UI Layout (aligned with MenuOverlay style)
    // =====================================================================

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- TopStatusBar ---
        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        // --- Title ---
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            text: {
                var tr = typeof translations !== "undefined" ? translations : null
                switch (addressScreen.phase) {
                case addressScreen.phaseCityLetters: return tr ? tr.navEnterCity : "Enter City"
                case addressScreen.phaseCityList: return tr ? tr.navSelectCity : "Select City"
                case addressScreen.phaseStreetLetters: return tr ? tr.navEnterStreet : "Enter Street"
                case addressScreen.phaseStreetList: return tr ? tr.navSelectStreet : "Select Street"
                case addressScreen.phaseHouseNumbers: return tr ? tr.navSelectNumber : "Select Number"
                case addressScreen.phaseConfirm: return tr ? tr.navConfirmDestination : "Confirm Destination"
                default: return "Destination"
                }
            }
            color: textPrimary
            font.pixelSize: themeStore.fontTitle
            font.weight: Font.Bold
        }

        // --- Breadcrumb ---
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 2
            visible: phase > phaseLoading
            text: {
                var parts = []
                if (addressScreen.selectedCity !== "")
                    parts.push(addressScreen.selectedCity)
                else if (addressScreen.cityPrefix !== "")
                    parts.push(addressScreen.cityPrefix + "_")

                if (addressScreen.phase >= addressScreen.phaseStreetLetters) {
                    if (addressScreen.selectedStreet !== "")
                        parts.push(addressScreen.selectedStreet)
                    else if (addressScreen.streetPrefix !== "")
                        parts.push(addressScreen.streetPrefix + "_")
                }

                if (addressScreen.phase === addressScreen.phaseCityLetters ||
                    addressScreen.phase === addressScreen.phaseStreetLetters) {
                    parts.push(addressScreen.matchCountText())
                }

                return parts.join(" › ")
            }
            color: textSecondary
            font.pixelSize: themeStore.fontBody
        }

        // --- Content area ---
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // --- Loading / Building state ---
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 16
                visible: dbStatus === statusLoading || dbStatus === statusBuilding

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof addressDatabase !== "undefined" ? addressDatabase.statusMessage : ""
                    color: textPrimary
                    font.pixelSize: themeStore.fontBody
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    visible: dbStatus === statusBuilding
                    width: 200
                    height: 6
                    radius: themeStore.radiusBar
                    color: surfaceColor

                    Rectangle {
                        width: parent.width * (typeof addressDatabase !== "undefined" ? addressDatabase.buildProgress : 0)
                        height: parent.height
                        radius: themeStore.radiusBar
                        color: textPrimary
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: dbStatus === statusBuilding
                    text: typeof addressDatabase !== "undefined"
                          ? Math.round(addressDatabase.buildProgress * 100) + "%"
                          : "0%"
                    color: textSecondary
                    font.pixelSize: themeStore.fontBody
                }
            }

            // --- Loading house numbers ---
            Text {
                anchors.centerIn: parent
                visible: loadingHouseNumbers
                text: typeof translations !== "undefined" ? translations.navLoadingHouseNumbers : "Loading..."
                color: textSecondary
                font.pixelSize: themeStore.fontBody
            }

            // --- Error state ---
            Text {
                anchors.centerIn: parent
                visible: dbStatus === statusError
                text: typeof addressDatabase !== "undefined" ? addressDatabase.statusMessage : "Address database unavailable"
                color: errorColor
                font.pixelSize: themeStore.fontBody
            }

            // --- Letter carousel (Phase 1 and 3) ---
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 20
                visible: (phase === phaseCityLetters || phase === phaseStreetLetters) && dbStatus === statusReady

                // Current prefix display
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        var prefix = addressScreen.phase === addressScreen.phaseCityLetters
                            ? addressScreen.cityPrefix : addressScreen.streetPrefix
                        return prefix + "_"
                    }
                    font.pixelSize: themeStore.fontHeading
                    font.weight: Font.Bold
                    color: textPrimary
                    font.letterSpacing: 2
                }

                // Character carousel with gradient sizing
                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 2
                    visible: addressScreen.validChars.length > 0

                    Repeater {
                        model: {
                            if (addressScreen.validChars.length === 0) return []
                            var chars = addressScreen.validChars
                            var idx = addressScreen.charIndex
                            var result = []
                            var window = 3
                            for (var i = -window; i <= window; i++) {
                                var ci = idx + i
                                if (ci >= 0 && ci < chars.length) {
                                    result.push({
                                        "char": chars[ci],
                                        "isCurrent": i === 0,
                                        "distance": Math.abs(i)
                                    })
                                }
                            }
                            return result
                        }

                        delegate: Rectangle {
                            // Smooth gradient: 56 → 48 → 42 → 36
                            readonly property int tileSize: modelData.isCurrent ? 56
                                : Math.max(36, 56 - modelData.distance * 8)
                            width: tileSize
                            height: tileSize
                            radius: themeStore.radiusCard
                            color: surfaceColor
                            border.width: modelData.isCurrent ? 2 : 0
                            border.color: isDark ? "#CCFFFFFF" : "#CC000000"
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    var c = modelData.char
                                    if (c === " ") return "␣"
                                    return c.toUpperCase()
                                }
                                // Smooth font: 24 → 20 → 17 → 14
                                font.pixelSize: modelData.isCurrent ? 24
                                    : Math.max(14, 24 - modelData.distance * 4)
                                font.weight: modelData.isCurrent ? Font.Bold : Font.Normal
                                color: textPrimary
                                opacity: modelData.isCurrent ? 1.0 : Math.max(0.35, 1.0 - modelData.distance * 0.25)
                            }
                        }
                    }
                }

                // Position indicator
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: addressScreen.validChars.length > 1
                    text: (addressScreen.charIndex + 1) + " / " + addressScreen.validChars.length
                    color: textTertiary
                    font.pixelSize: themeStore.fontBody
                }

                // No valid chars message
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: addressScreen.validChars.length === 0 && (
                        addressScreen.phase === addressScreen.phaseCityLetters
                            ? addressScreen.cityPrefix.length > 0
                            : addressScreen.streetPrefix.length > 0)
                    text: typeof translations !== "undefined" ? translations.navNoMatches : "No matches"
                    color: errorColor
                    font.pixelSize: themeStore.fontBody
                }
            }

            // --- List view (Phase 2, 4, 5) ---
            Item {
                anchors.fill: parent
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                visible: phase === phaseCityList || phase === phaseStreetList || phase === phaseHouseNumbers

                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: {
                            var items = addressScreen.itemList
                            var idx = addressScreen.listIndex
                            var maxVisible = 8
                            var result = []
                            var start = Math.max(0, idx - Math.floor(maxVisible / 2))
                            var end = Math.min(items.length, start + maxVisible)
                            start = Math.max(0, end - maxVisible)
                            for (var i = start; i < end; i++) {
                                result.push({"index": i, "item": items[i], "selected": i === idx})
                            }
                            return result
                        }

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            radius: themeStore.radiusCard
                            color: modelData.selected ? selectedBg : "transparent"

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                text: {
                                    var item = modelData.item
                                    if (item === undefined || item === null) return ""
                                    if (addressScreen.phase === addressScreen.phaseHouseNumbers) {
                                        return item.housenumber || ""
                                    } else if (addressScreen.phase === addressScreen.phaseStreetList) {
                                        var street = item.street || ""
                                        var postcode = item.postcode || ""
                                        var dupes = 0
                                        for (var j = 0; j < addressScreen.itemList.length; j++) {
                                            if (addressScreen.itemList[j].street === street) dupes++
                                        }
                                        if (dupes > 1 && postcode !== "")
                                            return street + " · " + postcode
                                        return street
                                    }
                                    return typeof item === "string" ? item : ""
                                }
                                color: textPrimary
                                font.pixelSize: themeStore.fontBody
                                font.weight: modelData.selected ? Font.Bold : Font.Normal
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                // Scroll up gradient
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 30
                    visible: addressScreen.listIndex > 0
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9) }
                        GradientStop { position: 1.0; color: isDark ? Qt.rgba(0, 0, 0, 0.0) : Qt.rgba(1, 1, 1, 0.0) }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: MaterialIcon.iconKeyboardArrowUp
                        font.family: "Material Icons"
                        font.pixelSize: themeStore.fontTitle
                        color: textSecondary
                    }
                }

                // Scroll down gradient
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 30
                    visible: addressScreen.listIndex < addressScreen.itemList.length - 1
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: isDark ? Qt.rgba(0, 0, 0, 0.0) : Qt.rgba(1, 1, 1, 0.0) }
                        GradientStop { position: 1.0; color: isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9) }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: MaterialIcon.iconKeyboardArrowDown
                        font.family: "Material Icons"
                        font.pixelSize: themeStore.fontTitle
                        color: textSecondary
                    }
                }
            }

            // --- Confirm view (Phase 6) ---
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 40
                spacing: 8
                visible: phase === phaseConfirm

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof translations !== "undefined"
                        ? translations.navConfirmDest : "DESTINATION"
                    color: textTertiary
                    font.pixelSize: themeStore.fontBody
                    font.letterSpacing: 1
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    text: {
                        var label = addressScreen.selectedStreet
                        if (addressScreen.selectedHouse !== "")
                            label += " " + addressScreen.selectedHouse
                        return label
                    }
                    font.pixelSize: themeStore.fontHeading
                    font.weight: Font.Bold
                    color: textPrimary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        var label = ""
                        if (addressScreen.selectedPostcode !== "")
                            label += addressScreen.selectedPostcode + " "
                        label += addressScreen.selectedCity
                        return label
                    }
                    font.pixelSize: themeStore.fontTitle
                    color: textSecondary
                }
            }
        }

        // --- Footer: ControlHints ---
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: controlHints.height
            color: "transparent"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: borderColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom

                leftAction: {
                    if (dbStatus !== statusReady) return ""
                    var tr = typeof translations !== "undefined" ? translations : null
                    if (addressScreen.phase === addressScreen.phaseConfirm)
                        return tr ? tr.controlBack : "Back"
                    return tr ? tr.controlScroll : "Scroll"
                }
                rightAction: {
                    var tr = typeof translations !== "undefined" ? translations : null
                    if (dbStatus === statusBuilding)
                        return tr ? tr.controlCancel : "Cancel"
                    if (dbStatus !== statusReady)
                        return tr ? tr.controlBack : "Close"
                    if (addressScreen.phase === addressScreen.phaseConfirm)
                        return tr ? tr.navGo : "Go!"
                    return tr ? tr.controlSelect : "Select"
                }
            }
        }
    }
}

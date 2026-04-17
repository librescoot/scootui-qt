import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/components"
import ScootUI 1.0

Rectangle {
    id: addressScreen
    color: ThemeStore.isDark ? "black" : "white"

    readonly property bool isDark: ThemeStore.isDark
    readonly property color textPrimary: addressScreen.isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: addressScreen.isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color textTertiary: addressScreen.isDark ? "#4DFFFFFF" : "#1F000000"
    readonly property color surfaceColor: addressScreen.isDark ? "#1E1E1E" : "#F5F5F5"
    readonly property color selectedBg: addressScreen.isDark ? "#3DFFFFFF" : "#1F000000"
    readonly property color borderColor: addressScreen.isDark ? "#1AFFFFFF" : "#1F000000"
    readonly property color errorColor: "#EF5350"
    readonly property color goColor: "#4CAF50"

    // Address database status constants (match C++ enum)
    readonly property int statusIdle: 0
    readonly property int statusLoading: 1
    readonly property int statusBuilding: 2
    readonly property int statusReady: 3
    readonly property int statusError: 4

    readonly property int dbStatus: AddressDatabaseService.status

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
    property int phase: addressScreen.phaseLoading
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
        if (addressScreen.dbStatus === addressScreen.statusReady) {
            enterCityLetters("")
        }
    }

    Component.onCompleted: {
        if (addressScreen.dbStatus === addressScreen.statusReady) {
            enterCityLetters("")
        } else if (addressScreen.dbStatus === addressScreen.statusIdle || addressScreen.dbStatus === addressScreen.statusError) {
            if (true)
                AddressDatabaseService.initialize()
        }
    }

    // --- Phase transition functions ---

    function enterCityLetters(prefix) {
        addressScreen.phase = addressScreen.phaseCityLetters
        addressScreen.cityPrefix = prefix
        addressScreen.charIndex = 0
        refreshValidChars()
    }

    function enterCityList(autoSelect) {
        addressScreen.phase = addressScreen.phaseCityList
        addressScreen.listIndex = 0
        addressScreen.itemList = AddressDatabaseService.getMatchingCities(addressScreen.cityPrefix)
        if (autoSelect !== false && addressScreen.itemList.length === 1) {
            selectCity(0)
        }
    }

    function enterStreetLetters(prefix) {
        addressScreen.phase = addressScreen.phaseStreetLetters
        addressScreen.streetPrefix = prefix
        addressScreen.charIndex = 0
        refreshValidChars()
    }

    function enterStreetList(autoSelect) {
        addressScreen.phase = addressScreen.phaseStreetList
        addressScreen.listIndex = 0
        addressScreen.itemList = AddressDatabaseService.getMatchingStreets(addressScreen.selectedCity, addressScreen.streetPrefix)
        if (autoSelect !== false && addressScreen.itemList.length === 1) {
            selectStreet(0)
        }
    }

    function enterHouseNumbers() {
        addressScreen.loadingHouseNumbers = true
        AddressDatabaseService.queryHouseNumbers(addressScreen.selectedCity, addressScreen.selectedStreet, addressScreen.selectedPostcode)
    }

    Connections {
        target: AddressDatabaseService

        function onHouseNumbersReady(houses) {
            addressScreen.loadingHouseNumbers = false
            if (houses.length <= 1) {
                if (houses.length === 1) {
                    addressScreen.selectedHouse = houses[0].housenumber
                    addressScreen.destLat = houses[0].latitude
                    addressScreen.destLng = houses[0].longitude
                } else {
                    addressScreen.selectedHouse = ""
                    var coords = AddressDatabaseService.getStreetCoordinates(
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
        addressScreen.phase = addressScreen.phaseConfirm
    }

    // --- Char carousel logic ---

    function refreshValidChars() {
        if (addressScreen.phase === addressScreen.phaseCityLetters) {
            addressScreen.validChars = AddressDatabaseService.getValidCityChars(addressScreen.cityPrefix)
        } else if (addressScreen.phase === addressScreen.phaseStreetLetters) {
            addressScreen.validChars = AddressDatabaseService.getValidStreetChars(addressScreen.selectedCity, addressScreen.streetPrefix)
        }
        addressScreen.charIndex = 0
    }

    function cycleChar() {
        if (addressScreen.validChars.length === 0) return
        addressScreen.charIndex = (addressScreen.charIndex + 1) % addressScreen.validChars.length
    }

    function selectCurrentChar() {
        if (addressScreen.validChars.length === 0) return

        var ch = addressScreen.validChars[addressScreen.charIndex]
        if (addressScreen.phase === addressScreen.phaseCityLetters) {
            addressScreen.cityPrefix += ch
            var cityCount = AddressDatabaseService.getCityCount(addressScreen.cityPrefix)
            if (cityCount <= addressScreen.maxListItems && cityCount > 0) {
                enterCityList()
            } else if (cityCount === 0) {
                addressScreen.cityPrefix = addressScreen.cityPrefix.slice(0, -1)
            } else {
                refreshValidChars()
            }
        } else if (addressScreen.phase === addressScreen.phaseStreetLetters) {
            addressScreen.streetPrefix += ch
            var streetCount = AddressDatabaseService.getStreetCount(addressScreen.selectedCity, addressScreen.streetPrefix)
            if (streetCount <= addressScreen.maxListItems && streetCount > 0) {
                enterStreetList()
            } else if (streetCount === 0) {
                addressScreen.streetPrefix = addressScreen.streetPrefix.slice(0, -1)
            } else {
                refreshValidChars()
            }
        }
    }

    function backspace() {
        if (addressScreen.phase === addressScreen.phaseCityLetters) {
            if (addressScreen.cityPrefix.length > 0) {
                addressScreen.cityPrefix = addressScreen.cityPrefix.slice(0, -1)
                refreshValidChars()
            } else {
                if (true)
                    ScreenStore.setScreen(1)
            }
        } else if (addressScreen.phase === addressScreen.phaseStreetLetters) {
            if (addressScreen.streetPrefix.length > 0) {
                addressScreen.streetPrefix = addressScreen.streetPrefix.slice(0, -1)
                refreshValidChars()
            } else {
                enterCityList(false)
            }
        }
    }

    // --- List selection logic ---

    function cycleListItem() {
        if (addressScreen.itemList.length === 0) return
        addressScreen.listIndex = (addressScreen.listIndex + 1) % addressScreen.itemList.length
    }

    function selectCity(index) {
        if (index === undefined) index = addressScreen.listIndex
        if (index < 0 || index >= addressScreen.itemList.length) return
        addressScreen.selectedCity = addressScreen.itemList[index]
        addressScreen.streetPrefix = ""
        enterStreetLetters("")
    }

    function selectStreet(index) {
        if (index === undefined) index = addressScreen.listIndex
        if (index < 0 || index >= addressScreen.itemList.length) return
        var entry = addressScreen.itemList[index]
        addressScreen.selectedStreet = entry.street
        addressScreen.selectedPostcode = entry.postcode || ""
        enterHouseNumbers()
    }

    function selectHouseNumber() {
        if (addressScreen.listIndex < 0 || addressScreen.listIndex >= addressScreen.itemList.length) return
        var entry = addressScreen.itemList[addressScreen.listIndex]
        addressScreen.selectedHouse = entry.housenumber
        addressScreen.destLat = entry.latitude
        addressScreen.destLng = entry.longitude
        enterConfirm()
    }

    function confirmAndNavigate() {
        var addressLabel = addressScreen.selectedStreet
        if (addressScreen.selectedHouse !== "")
            addressLabel += " " + addressScreen.selectedHouse
        addressLabel += ", " + addressScreen.selectedCity

        if (true) {
            NavigationService.setDestination(addressScreen.destLat, addressScreen.destLng, addressLabel)
        }
        if (true) {
            ScreenStore.setScreen(1)
        }
    }

    function matchCountText() {
        var tr = Translations
        if (addressScreen.phase === addressScreen.phaseCityLetters) {
            var count = AddressDatabaseService.getCityCount(addressScreen.cityPrefix)
            var label = tr ? tr.navCities : "cities"
            return count + " " + label
        } else if (addressScreen.phase === addressScreen.phaseStreetLetters) {
            var scount = AddressDatabaseService.getStreetCount(addressScreen.selectedCity, addressScreen.streetPrefix)
            var slabel = tr ? tr.navStreets : "streets"
            return scount + " " + slabel
        }
        return ""
    }

    // --- Input handling ---

    Connections {
        target: InputHandler

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
                if (true)
                    AddressDatabaseService.cancelBuild()
                return
            }
            if (addressScreen.dbStatus !== addressScreen.statusReady) {
                if (true)
                    ScreenStore.setScreen(1)
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
                var tr = Translations
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
            color: addressScreen.textPrimary
            font.pixelSize: ThemeStore.fontTitle
            font.weight: Font.Bold
        }

        // --- Breadcrumb ---
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 2
            visible: addressScreen.phase > addressScreen.phaseLoading
            text: {
                var result = ""
                var sep = " › "
                if (addressScreen.selectedCity !== "")
                    result += addressScreen.selectedCity
                else if (addressScreen.cityPrefix !== "")
                    result += addressScreen.cityPrefix + "_"

                if (addressScreen.phase >= addressScreen.phaseStreetLetters) {
                    if (addressScreen.selectedStreet !== "")
                        result += (result ? sep : "") + addressScreen.selectedStreet
                    else if (addressScreen.streetPrefix !== "")
                        result += (result ? sep : "") + addressScreen.streetPrefix + "_"
                }

                if (addressScreen.phase === addressScreen.phaseCityLetters ||
                    addressScreen.phase === addressScreen.phaseStreetLetters) {
                    result += (result ? sep : "") + addressScreen.matchCountText()
                }

                return result
            }
            color: addressScreen.textSecondary
            font.pixelSize: ThemeStore.fontBody
        }

        // --- Content area ---
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // --- Loading / Building state ---
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 16
                visible: addressScreen.dbStatus === addressScreen.statusLoading || addressScreen.dbStatus === addressScreen.statusBuilding

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: AddressDatabaseService.statusMessage
                    color: addressScreen.textPrimary
                    font.pixelSize: ThemeStore.fontBody
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    visible: addressScreen.dbStatus === addressScreen.statusBuilding
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 6
                    radius: ThemeStore.radiusBar
                    color: addressScreen.surfaceColor

                    Rectangle {
                        width: parent.width * (AddressDatabaseService.buildProgress)
                        height: parent.height
                        radius: ThemeStore.radiusBar
                        color: addressScreen.textPrimary
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: addressScreen.dbStatus === addressScreen.statusBuilding
                    text: true
                          ? Math.round(AddressDatabaseService.buildProgress * 100) + "%"
                          : "0%"
                    color: addressScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                }
            }

            // --- Loading house numbers ---
            Text {
                anchors.centerIn: parent
                visible: addressScreen.loadingHouseNumbers
                text: Translations.navLoadingHouseNumbers
                color: addressScreen.textSecondary
                font.pixelSize: ThemeStore.fontBody
            }

            // --- Error state ---
            Text {
                anchors.centerIn: parent
                visible: addressScreen.dbStatus === addressScreen.statusError
                text: AddressDatabaseService.statusMessage
                color: addressScreen.errorColor
                font.pixelSize: ThemeStore.fontBody
            }

            // --- Letter carousel (Phase 1 and 3) ---
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 20
                visible: (addressScreen.phase === addressScreen.phaseCityLetters || addressScreen.phase === addressScreen.phaseStreetLetters) && addressScreen.dbStatus === addressScreen.statusReady

                // Current prefix display
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        var prefix = addressScreen.phase === addressScreen.phaseCityLetters
                            ? addressScreen.cityPrefix : addressScreen.streetPrefix
                        return prefix + "_"
                    }
                    font.pixelSize: ThemeStore.fontHeading
                    font.weight: Font.Bold
                    color: addressScreen.textPrimary
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
                            id: charTile
                            required property var modelData
                            // Smooth gradient: 56 → 48 → 42 → 36
                            readonly property int tileSize: charTile.modelData.isCurrent ? 56
                                : Math.max(36, 56 - charTile.modelData.distance * 8)
                            width: tileSize
                            height: tileSize
                            radius: ThemeStore.radiusCard
                            color: ThemeStore.isDark ? "#1E1E1E" : "#F5F5F5"
                            border.width: charTile.modelData.isCurrent ? 2 : 0
                            border.color: ThemeStore.isDark ? "#CCFFFFFF" : "#CC000000"
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    var c = charTile.modelData.char
                                    if (c === " ") return "␣"
                                    return c.toUpperCase()
                                }
                                // Smooth font: 24 → 20 → 17 → 14
                                font.pixelSize: charTile.modelData.isCurrent ? 24
                                    : Math.max(14, 24 - charTile.modelData.distance * 4)
                                font.weight: charTile.modelData.isCurrent ? Font.Bold : Font.Normal
                                color: ThemeStore.isDark ? "#FFFFFF" : "#000000"
                                opacity: charTile.modelData.isCurrent ? 1.0 : Math.max(0.35, 1.0 - charTile.modelData.distance * 0.25)
                            }
                        }
                    }
                }

                // Position indicator
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: addressScreen.validChars.length > 1
                    text: (addressScreen.charIndex + 1) + " / " + addressScreen.validChars.length
                    color: addressScreen.textTertiary
                    font.pixelSize: ThemeStore.fontBody
                }

                // No valid chars message
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: addressScreen.validChars.length === 0 && (
                        addressScreen.phase === addressScreen.phaseCityLetters
                            ? addressScreen.cityPrefix.length > 0
                            : addressScreen.streetPrefix.length > 0)
                    text: Translations.navNoMatches
                    color: addressScreen.errorColor
                    font.pixelSize: ThemeStore.fontBody
                }
            }

            // --- List view (Phase 2, 4, 5) ---
            Item {
                anchors.fill: parent
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                visible: addressScreen.phase === addressScreen.phaseCityList || addressScreen.phase === addressScreen.phaseStreetList || addressScreen.phase === addressScreen.phaseHouseNumbers

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
                            id: listItem
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            radius: ThemeStore.radiusCard
                            color: listItem.modelData.selected ? (ThemeStore.isDark ? "#3DFFFFFF" : "#1F000000") : "transparent"

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                text: {
                                    var item = listItem.modelData.item
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
                                color: ThemeStore.isDark ? "#FFFFFF" : "#000000"
                                font.pixelSize: ThemeStore.fontBody
                                font.weight: listItem.modelData.selected ? Font.Bold : Font.Normal
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
                        GradientStop { position: 0.0; color: addressScreen.isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9) }
                        GradientStop { position: 1.0; color: addressScreen.isDark ? Qt.rgba(0, 0, 0, 0.0) : Qt.rgba(1, 1, 1, 0.0) }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: MaterialIcon.iconKeyboardArrowUp
                        font.family: "Material Icons"
                        font.pixelSize: ThemeStore.fontTitle
                        color: addressScreen.textSecondary
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
                        GradientStop { position: 0.0; color: addressScreen.isDark ? Qt.rgba(0, 0, 0, 0.0) : Qt.rgba(1, 1, 1, 0.0) }
                        GradientStop { position: 1.0; color: addressScreen.isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9) }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: MaterialIcon.iconKeyboardArrowDown
                        font.family: "Material Icons"
                        font.pixelSize: ThemeStore.fontTitle
                        color: addressScreen.textSecondary
                    }
                }
            }

            // --- Confirm view (Phase 6) ---
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 40
                spacing: 8
                visible: addressScreen.phase === addressScreen.phaseConfirm

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: Translations.navConfirmDest
                    color: addressScreen.textTertiary
                    font.pixelSize: ThemeStore.fontBody
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
                    font.pixelSize: ThemeStore.fontHeading
                    font.weight: Font.Bold
                    color: addressScreen.textPrimary
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
                    font.pixelSize: ThemeStore.fontTitle
                    color: addressScreen.textSecondary
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
                color: addressScreen.borderColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom

                leftAction: {
                    if (addressScreen.dbStatus !== addressScreen.statusReady) return ""
                    var tr = Translations
                    if (addressScreen.phase === addressScreen.phaseConfirm)
                        return tr ? tr.controlBack : "Back"
                    return tr ? tr.controlScroll : "Scroll"
                }
                rightAction: {
                    var tr = Translations
                    if (addressScreen.dbStatus === addressScreen.statusBuilding)
                        return tr ? tr.controlCancel : "Cancel"
                    if (addressScreen.dbStatus !== addressScreen.statusReady)
                        return tr ? tr.controlBack : "Close"
                    if (addressScreen.phase === addressScreen.phaseConfirm)
                        return tr ? tr.navGo : "Go!"
                    return tr ? tr.controlSelect : "Select"
                }
            }
        }
    }
}

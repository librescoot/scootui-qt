import QtQuick
import QtQuick.Layouts
import ScootUI 1.0
import "../widgets/status_bars"
import "../widgets/components"

// View for AddressEntryController: renders the controller's state and maps
// brake gestures onto its three inputs. All entry logic lives in C++.
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

    readonly property int dbStatus: typeof addressDatabase !== "undefined" ? addressDatabase.status : AddressDatabaseService.Error

    readonly property int phase: typeof addressEntry !== "undefined" ? addressEntry.phase : AddressEntryController.Loading
    readonly property bool loadingHouseNumbers: typeof addressEntry !== "undefined" && addressEntry.loadingHouseNumbers

    readonly property bool inLetterPhase: phase === AddressEntryController.CityLetters
                                          || phase === AddressEntryController.StreetLetters
                                          || phase === AddressEntryController.HouseDigits
    readonly property bool inListPhase: phase === AddressEntryController.CityList
                                        || phase === AddressEntryController.StreetList
                                        || phase === AddressEntryController.HouseNumbers

    readonly property string activePrefix: {
        if (typeof addressEntry === "undefined") return ""
        if (phase === AddressEntryController.CityLetters) return addressEntry.cityPrefix
        if (phase === AddressEntryController.StreetLetters) return addressEntry.streetPrefix
        return addressEntry.housePrefix
    }

    Component.onCompleted: {
        if (typeof addressEntry !== "undefined")
            addressEntry.activate()
    }

    function matchCountText() {
        var tr = typeof translations !== "undefined" ? translations : null
        var count = addressEntry.matchCount
        if (phase === AddressEntryController.CityLetters)
            return count + " " + (tr ? tr.navCities : "cities")
        if (phase === AddressEntryController.StreetLetters)
            return count + " " + (tr ? tr.navStreets : "streets")
        if (phase === AddressEntryController.HouseDigits)
            return count + " " + (tr ? tr.navHouses : "houses")
        return ""
    }

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null

        function onLeftTap() {
            if (typeof addressEntry !== "undefined")
                addressEntry.scroll()
        }

        function onLeftHold() {
            if (typeof addressEntry !== "undefined")
                addressEntry.back()
        }

        function onRightTap() {
            if (typeof addressEntry !== "undefined")
                addressEntry.select()
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
                case AddressEntryController.CityLetters: return tr ? tr.navEnterCity : "Enter City"
                case AddressEntryController.CityList: return tr ? tr.navSelectCity : "Select City"
                case AddressEntryController.StreetLetters: return tr ? tr.navEnterStreet : "Enter Street"
                case AddressEntryController.StreetList: return tr ? tr.navSelectStreet : "Select Street"
                case AddressEntryController.HouseDigits: return tr ? tr.navEnterNumber : "Enter Number"
                case AddressEntryController.HouseNumbers: return tr ? tr.navSelectNumber : "Select Number"
                case AddressEntryController.Confirm: return tr ? tr.navConfirmDestination : "Confirm Destination"
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
            visible: addressScreen.phase > AddressEntryController.Loading
            text: {
                if (typeof addressEntry === "undefined") return ""
                var parts = []
                if (addressEntry.selectedCity !== "")
                    parts.push(addressEntry.selectedCity)
                else if (addressEntry.cityPrefix !== "")
                    parts.push(addressEntry.cityPrefix + "_")

                if (addressScreen.phase >= AddressEntryController.StreetLetters
                    && addressScreen.phase !== AddressEntryController.HouseDigits) {
                    if (addressEntry.selectedStreet !== "")
                        parts.push(addressEntry.selectedStreet)
                    else if (addressEntry.streetPrefix !== "")
                        parts.push(addressEntry.streetPrefix + "_")
                }
                if (addressScreen.phase === AddressEntryController.HouseDigits) {
                    if (addressEntry.selectedStreet !== "")
                        parts.push(addressEntry.selectedStreet)
                    parts.push(addressEntry.housePrefix + "_")
                }

                if (addressScreen.inLetterPhase)
                    parts.push(addressScreen.matchCountText())

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
                visible: dbStatus === AddressDatabaseService.Loading || dbStatus === AddressDatabaseService.Building

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof addressDatabase !== "undefined" ? addressDatabase.statusMessage : ""
                    color: textPrimary
                    font.pixelSize: themeStore.fontBody
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    visible: dbStatus === AddressDatabaseService.Building
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
                    visible: dbStatus === AddressDatabaseService.Building
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
                visible: dbStatus === AddressDatabaseService.Error
                text: typeof addressDatabase !== "undefined" ? addressDatabase.statusMessage : "Address database unavailable"
                color: errorColor
                font.pixelSize: themeStore.fontBody
            }

            // --- Letter carousel ---
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 20
                visible: addressScreen.inLetterPhase && dbStatus === AddressDatabaseService.Ready

                // Current prefix display
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: addressScreen.activePrefix + "_"
                    font.pixelSize: themeStore.fontHeading
                    font.weight: Font.Bold
                    color: textPrimary
                    font.letterSpacing: 2
                }

                // Character carousel with gradient sizing
                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 2
                    visible: typeof addressEntry !== "undefined" && addressEntry.validChars.length > 0

                    Repeater {
                        model: {
                            if (typeof addressEntry === "undefined") return []
                            var chars = addressEntry.validChars
                            if (chars.length === 0) return []
                            var idx = addressEntry.charIndex
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
                    visible: typeof addressEntry !== "undefined" && addressEntry.validChars.length > 1
                    text: typeof addressEntry !== "undefined"
                          ? (addressEntry.charIndex + 1) + " / " + addressEntry.validChars.length
                          : ""
                    color: textTertiary
                    font.pixelSize: themeStore.fontBody
                }

                // No valid chars message
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: typeof addressEntry !== "undefined"
                             && addressEntry.validChars.length === 0
                             && addressScreen.activePrefix.length > 0
                    text: typeof translations !== "undefined" ? translations.navNoMatches : "No matches"
                    color: errorColor
                    font.pixelSize: themeStore.fontBody
                }
            }

            // --- List view ---
            Item {
                anchors.fill: parent
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                visible: addressScreen.inListPhase

                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: {
                            if (typeof addressEntry === "undefined") return []
                            var items = addressEntry.itemList
                            var idx = addressEntry.listIndex
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
                                    if (addressScreen.phase === AddressEntryController.HouseNumbers) {
                                        return item.housenumber || ""
                                    } else if (addressScreen.phase === AddressEntryController.StreetList) {
                                        var street = item.street || ""
                                        var postcode = item.postcode || ""
                                        var list = addressEntry.itemList
                                        var dupes = 0
                                        for (var j = 0; j < list.length; j++) {
                                            if (list[j].street === street) dupes++
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
                    visible: typeof addressEntry !== "undefined" && addressEntry.listIndex > 0
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
                    visible: typeof addressEntry !== "undefined"
                             && addressEntry.listIndex < addressEntry.itemList.length - 1
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

            // --- Confirm view ---
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 40
                spacing: 8
                visible: addressScreen.phase === AddressEntryController.Confirm

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
                        if (typeof addressEntry === "undefined") return ""
                        var label = addressEntry.selectedStreet
                        if (addressEntry.selectedHouse !== "")
                            label += " " + addressEntry.selectedHouse
                        return label
                    }
                    font.pixelSize: themeStore.fontHeading
                    font.weight: Font.Bold
                    color: textPrimary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        if (typeof addressEntry === "undefined") return ""
                        var label = ""
                        if (addressEntry.selectedPostcode !== "")
                            label += addressEntry.selectedPostcode + " "
                        label += addressEntry.selectedCity
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

                // The tap cycles letters and list entries; it does nothing on
                // the confirm step, where only the hold is bound.
                leftTap: {
                    if (dbStatus !== AddressDatabaseService.Ready) return ""
                    if (addressScreen.phase === AddressEntryController.Confirm) return ""
                    var tr = typeof translations !== "undefined" ? translations : null
                    return tr ? tr.controlScroll : "Scroll"
                }
                leftHold: {
                    if (dbStatus !== AddressDatabaseService.Ready) return ""
                    var tr = typeof translations !== "undefined" ? translations : null
                    return tr ? tr.controlBack : "Back"
                }
                rightTap: {
                    var tr = typeof translations !== "undefined" ? translations : null
                    if (dbStatus === AddressDatabaseService.Building)
                        return tr ? tr.controlCancel : "Cancel"
                    if (dbStatus !== AddressDatabaseService.Ready)
                        return tr ? tr.controlBack : "Close"
                    if (addressScreen.phase === AddressEntryController.Confirm)
                        return tr ? tr.navGo : "Go!"
                    return tr ? tr.controlSelect : "Select"
                }
            }
        }
    }
}

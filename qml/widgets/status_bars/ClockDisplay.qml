import QtQuick
import "../indicators"
import ScootUI 1.0

// Centre element of the top status bar. dashboard.show-clock picks the format:
//
//   always     time only. The historical value, kept so an existing setting
//              keeps meaning what it meant before there were formats.
//   date-time  calendar glyph carrying the day, then the time
//   alternate  time, then day and month, swapping on a timer
//   never      hidden
//
// The calendar glyph appears only in date-time, where the date is an icon
// sitting beside other content and has to say what it is. Standing in for the
// clock in alternate, the date is set as text in the clock's own style: a
// framed numeral there would read as a status icon rather than as the time.
//
// reservedWidth is the width this element claims whatever it is currently
// showing. TopStatusBar derives both side budgets from it, so an alternating
// centre must not make the battery and indicator rows degrade and un-degrade
// on every swap.
Item {
    id: root

    readonly property real iconSize: 26
    readonly property real gap: 5

    // The time is what a rider glances down for; the date is a courtesy. Give
    // it a short turn rather than an equal one.
    readonly property int timePhaseMs: 10000
    readonly property int datePhaseMs: 4000
    readonly property int fadeMs: 200

    // The dashboard can boot before NTP or a GPS fix and sit on a 1970 clock.
    // A wrong time reads as merely wrong; a wrong date reads as broken. Hold
    // the date back until the clock is plausible and show time alone until
    // then. Same cutoff as the GPS clock check in NavigationService.
    readonly property int minPlausibleYear: 2025

    property date now: new Date()

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }

    // `simulator` is set to nullptr on non-simulator builds rather than left
    // undefined (Application.cpp), and `typeof null` is "object", so the null
    // check is the one that matters here. Main.qml guards the same way.
    readonly property string clockOverride:
        (typeof simulator !== "undefined" && simulator !== null) ? simulator.clockOverride : ""
    readonly property string dateOverride:
        (typeof simulator !== "undefined" && simulator !== null) ? simulator.dateOverride : ""

    // Parsed by hand rather than through Date.fromLocaleDateString so the
    // simulator field means the same thing under either UI language.
    readonly property date effectiveDate: {
        var m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(dateOverride)
        if (m)
            return new Date(parseInt(m[1], 10), parseInt(m[2], 10) - 1, parseInt(m[3], 10))
        return now
    }

    readonly property bool dateIsPlausible: effectiveDate.getFullYear() >= minPlausibleYear

    readonly property string setting: {
        var s = typeof SettingsStore !== "undefined" ? SettingsStore.showClock : "always"
        return s === "" ? "always" : s
    }

    readonly property string mode: {
        if (setting === "never")
            return "never"
        if (setting === "date-time" || setting === "alternate")
            return dateIsPlausible ? setting : "always"
        return "always"
    }

    readonly property string timeStr: {
        if (clockOverride.length > 0)
            return clockOverride
        return ("0" + now.getHours()).slice(-2) + ":" + ("0" + now.getMinutes()).slice(-2)
    }
    readonly property int dayNum: effectiveDate.getDate()
    readonly property string monthStr: {
        // A Q_INVOKABLE call carries no binding dependency of its own; naming
        // the language is what re-runs this when the rider switches it.
        Translations.language
        return Translations.monthAbbrev(effectiveDate.getMonth() + 1)
    }
    // German wants the ordinal point on the day ("17. Aug"), English does not.
    readonly property string dateStr: Translations.dateDayMonth.arg(dayNum).arg(monthStr)

    property bool showingDate: false

    Timer {
        running: root.mode === "alternate"
        repeat: true
        interval: root.showingDate ? root.datePhaseMs : root.timePhaseMs
        onTriggered: root.showingDate = !root.showingDate
    }

    onModeChanged: if (mode !== "alternate") showingDate = false

    // Both faces are measured whether or not they are on screen, so the
    // reserved width is a constant of the format rather than of the phase.
    TextMetrics {
        id: tmTime
        font.pixelSize: ThemeStore.fontTitle
        font.weight: Font.Medium
        font.features: {"tnum": 1}
        text: root.timeStr
    }
    TextMetrics {
        id: tmDate
        font.pixelSize: ThemeStore.fontTitle
        font.weight: Font.Medium
        font.features: {"tnum": 1}
        text: root.dateStr
    }

    readonly property real reservedWidth: {
        switch (mode) {
        case "never":     return 0
        case "date-time": return iconSize + gap + tmTime.width
        case "alternate": return Math.max(tmTime.width, tmDate.width)
        default:          return tmTime.width
        }
    }

    visible: mode !== "never"
    implicitWidth: reservedWidth
    implicitHeight: Math.max(iconSize, tmTime.height)

    // Time face. Carries the glyph as well in date-time, where day and time
    // are shown together and the month would crowd the bar.
    Row {
        anchors.centerIn: parent
        height: root.height
        spacing: root.gap
        opacity: root.showingDate ? 0 : 1
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: root.fadeMs } }

        CalendarDayIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.mode === "date-time"
            day: root.dayNum
            iconSize: root.iconSize
            color: ThemeStore.textColor
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.timeStr
            font.pixelSize: ThemeStore.fontTitle
            font.weight: Font.Medium
            // Tabular figures: without them the clock changes width as the
            // digits change, which moves the side budgets under it.
            font.features: {"tnum": 1}
            color: ThemeStore.textColor
        }
    }

    // Date face, only ever shown by the alternating format. Plain text in the
    // clock's own style, since here it stands in for the clock.
    Text {
        anchors.centerIn: parent
        text: root.dateStr
        font.pixelSize: ThemeStore.fontTitle
        font.weight: Font.Medium
        font.features: {"tnum": 1}
        color: ThemeStore.textColor
        opacity: root.showingDate ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: root.fadeMs } }
    }
}

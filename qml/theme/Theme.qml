pragma Singleton
import QtQuick

QtObject {
    id: theme

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true

    // Primary text
    readonly property color textColor: isDark ? "#FFFFFF" : "#000000"
    // Secondary text (60% / 54%)
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    // Tertiary text (30% / 12%)
    readonly property color textTertiary: isDark ? "#4DFFFFFF" : "#1F000000"
    // Hint text (54% / 38%)
    readonly property color textHint: isDark ? "#8AFFFFFF" : "#8A000000"

    // Backgrounds
    readonly property color backgroundColor: isDark ? "#000000" : "#FFFFFF"
    readonly property color surfaceColor: isDark ? "#1E1E1E" : "#F5F5F5"

    // Borders
    readonly property color borderColor: isDark ? "#1AFFFFFF" : "#1F000000"

    // Speedometer colors
    readonly property color speedArcBlue: "#2196F3"
    readonly property color speedArcPurple: "#9C27B0"
    readonly property color speedArcPink: "#E91E63"
    readonly property color speedArcBackground: isDark ? "#424242" : "#E0E0E0"
    readonly property color regenRed: "#4DFF0000"

    // Battery colors
    readonly property color batteryRed: "#FF0000"
    readonly property color batteryOrange: "#FF7900"

    // Power display
    readonly property color powerRegen: "#43A047"
    readonly property color powerDischarge: "#1E88E5"
    readonly property color powerBoost: "#FB8C00"
    readonly property color powerBarBg: isDark ? "#424242" : "#E0E0E0"
    readonly property color powerZeroMark: isDark ? "#66FFFFFF" : "#61000000"

    // Indicator colors
    readonly property color indicatorGreen: "#4CAF50"
    readonly property color indicatorRed: "#F44336"
    readonly property color indicatorYellow: "#FFC107"
    readonly property color indicatorOrange: "#FF9800"

    // Font sizes (scaled)
    readonly property real fontSpeedMain: 96
    readonly property real fontSpeedUnit: 22
    readonly property real fontSpeedLabel: 11
    readonly property real fontStatusValue: 16
    readonly property real fontStatusLabel: 10
    readonly property real fontClock: 24
}

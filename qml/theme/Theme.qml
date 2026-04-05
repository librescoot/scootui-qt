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

    // Type scale
    readonly property real fontDisplay: 96      // Speedometer main
    readonly property real fontPin: 80          // BT PIN (special)
    readonly property real fontHero: 64         // Large icons, countdown
    readonly property real fontXL: 48           // Speed compact, GPS icon
    readonly property real fontFeature: 36      // ShortcutMenu selected, coordinates
    readonly property real fontInput: 32        // Address keyboard, headings
    readonly property real fontHeading: 28      // Overlay/menu titles, clock
    readonly property real fontTitle: 20        // Screen titles, menu items
    readonly property real fontBody: 18         // Body text, values, labels
    readonly property real fontCaption: 14      // Status bar labels, scale bar, road name
    readonly property real fontMicro: 10        // Status indicator labels

    // Border radii
    readonly property real radiusBar: 2         // Progress bars, thin indicators
    readonly property real radiusCard: 8        // Cards, menu items, toasts, badges
    readonly property real radiusModal: 16      // Modals, action buttons, large containers
}

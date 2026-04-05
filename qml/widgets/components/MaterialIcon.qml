import QtQuick
import "../../theme"

Text {
    id: root

    // Material Icons codepoints (baseline variants)
    // Navigation / Turn-by-turn
    // Note: These icons are in the Unicode supplementary plane (U+F0XXX).
    // QML's \uXXXX escape only handles 4 hex digits, so we use String.fromCodePoint().
    readonly property string iconTurnLeft:        String.fromCodePoint(0xf058f)
    readonly property string iconTurnRight:       String.fromCodePoint(0xf0590)
    readonly property string iconTurnSharpLeft:   String.fromCodePoint(0xf0591)
    readonly property string iconTurnSharpRight:  String.fromCodePoint(0xf0592)
    readonly property string iconTurnSlightLeft:  String.fromCodePoint(0xf0593)
    readonly property string iconTurnSlightRight: String.fromCodePoint(0xf0594)
    readonly property string iconUTurnLeft:       String.fromCodePoint(0xf0595)
    readonly property string iconUTurnRight:      String.fromCodePoint(0xf0596)
    readonly property string iconStraight:        String.fromCodePoint(0xf0574)
    readonly property string iconMerge:           String.fromCodePoint(0xf053b)
    readonly property string iconNavigation:      "\ue41e"
    readonly property string iconPlace:           "\ue4c9"

    // Arrows
    readonly property string iconArrowBack:       "\ue092"
    readonly property string iconArrowForward:    "\ue09b"
    readonly property string iconChevronLeft:     "\ue15e"
    readonly property string iconChevronRight:    "\ue15f"
    readonly property string iconKeyboardArrowDown: "\ue353"
    readonly property string iconKeyboardArrowUp:   "\ue356"

    // Actions / Controls
    readonly property string iconCheck:           "\ue156"
    readonly property string iconClose:           "\ue16a"
    readonly property string iconCheckCircleOutline: "\ue15a"
    readonly property string iconRefresh:         "\ue514"
    readonly property string iconPowerSettingsNew: "\ue4e3"

    // Status / Info
    readonly property string iconErrorOutline:    "\ue238"
    readonly property string iconWarningAmber:    "\ue6cc"
    readonly property string iconSpeed:           "\ue5e0"
    readonly property string iconTimer:           "\ue662"
    readonly property string iconFlag:            "\ue28e"
    readonly property string iconGpsNotFixed:     "\ue2dd"
    readonly property string iconLinkOff:         "\ue381"

    // Theme / Display
    readonly property string iconLightMode:       "\ue37a"
    readonly property string iconDarkMode:        "\ue1b0"
    readonly property string iconContrast:        String.fromCodePoint(0xf04d8)
    readonly property string iconMap:             "\uf1ae"

    // Misc
    readonly property string iconBugReport:       "\ue115"

    font.family: "Material Icons"
    font.pixelSize: Theme.fontTitle
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}

pragma Singleton
import QtQuick

// Central Material Icons codepoint registry.
//
// Single source of truth for Material Icons glyphs used across the UI.
// Every QML file should reference these instead of inlining "\ueXXX" literals —
// raw codepoints silently rot across font revisions (that's how we ended up
// rendering a cigarette for arrow_forward).
//
// To verify or regenerate, run:
//   python3 -c "from fontTools.ttLib import TTFont; \
//     print({v: hex(k) for k,v in TTFont('assets/fonts/MaterialIcons-Regular.otf').getBestCmap().items()})"
//
// Font: assets/fonts/MaterialIcons-Regular.otf
QtObject {
    // Navigation / Turn-by-turn
    // Supplementary-plane (U+F0XXX) — \uXXXX can't encode these, so use fromCodePoint.
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
    readonly property string iconForkLeft:        String.fromCodePoint(0xf050b)
    readonly property string iconForkRight:       String.fromCodePoint(0xf050c)
    readonly property string iconNavigation:      "\ue41e"
    readonly property string iconPlace:           "\ue4c9"
    readonly property string iconLocationOff:     "\ue3aa"
    readonly property string iconGpsNotFixed:     "\ue2dd"

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
    readonly property string iconCancel:          "\ue139"
    readonly property string iconCheckCircleOutline: "\ue15a"
    readonly property string iconRefresh:         "\ue514"
    readonly property string iconPowerSettingsNew: "\ue4e3"
    readonly property string iconLock:            "\ue3ae"

    // Status / Info
    readonly property string iconErrorOutline:    "\ue238"
    readonly property string iconWarningAmber:    "\ue6cc"
    readonly property string iconSpeed:           "\ue5e0"
    readonly property string iconTimer:           "\ue662"
    readonly property string iconFlag:            "\ue28e"
    readonly property string iconLinkOff:         "\ue381"
    readonly property string iconUsb:             "\ue697"

    // Theme / Display
    readonly property string iconLightMode:       "\ue37a"
    readonly property string iconDarkMode:        "\ue1b0"
    readonly property string iconContrast:        String.fromCodePoint(0xf04d8)
    readonly property string iconMap:             "\uf1ae" // map_outlined

    // Updates
    readonly property string iconUpdate:          "\ue692"
    readonly property string iconCloudDownload:   "\ue172"

    // Misc
    readonly property string iconBugReport:       "\ue115"
}

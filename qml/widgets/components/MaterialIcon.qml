import QtQuick

Text {
    id: root

    // Material Icons codepoints (baseline variants)
    // Navigation / Turn-by-turn
    readonly property string iconTurnLeft:        "\uf058f"
    readonly property string iconTurnRight:       "\uf0590"
    readonly property string iconTurnSharpLeft:   "\uf0591"
    readonly property string iconTurnSharpRight:  "\uf0592"
    readonly property string iconTurnSlightLeft:  "\uf0593"
    readonly property string iconTurnSlightRight: "\uf0594"
    readonly property string iconUTurnLeft:       "\uf0595"
    readonly property string iconUTurnRight:      "\uf0596"
    readonly property string iconStraight:        "\uf0574"
    readonly property string iconMerge:           "\uf053b"
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
    readonly property string iconContrast:        "\uf04d8"
    readonly property string iconMap:             "\uf1ae"

    // Misc
    readonly property string iconBugReport:       "\ue115"

    font.family: "Material Icons"
    font.pixelSize: 24
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}

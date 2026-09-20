pragma Singleton
import QtQuick

// Central Material Symbols codepoint registry.
QtObject {
    // Navigation / Turn-by-turn
    readonly property string iconTurnLeft:        "\ueba6"
    readonly property string iconTurnRight:       "\uebab"
    readonly property string iconTurnSharpLeft:   "\ueba7"
    readonly property string iconTurnSharpRight:  "\uebaa"
    readonly property string iconTurnSlightLeft:  "\ueba4"
    readonly property string iconTurnSlightRight: "\ueb9a"
    readonly property string iconUTurnLeft:       "\ueba1"
    readonly property string iconUTurnRight:      "\ueba2"
    readonly property string iconStraight:        "\ueb95"
    readonly property string iconMerge:           "\ueb98"
    readonly property string iconForkLeft:        "\ueba0"
    readonly property string iconForkRight:       "\uebac"
    readonly property string iconNavigation:      "\ue55d"
    readonly property string iconRoute:           "\ueacd"
    readonly property string iconPlace:           "\uf1db"
    readonly property string iconHome:            "\ue9b2"
    readonly property string iconWork:            "\ue943"
    readonly property string iconStar:            "\uf09a"
    readonly property string iconLocationOff:     "\ue0c7"
    readonly property string iconGpsNotFixed:     "\ue1b7"

    // Arrows
    readonly property string iconArrowBack:       "\ue5c4"
    readonly property string iconArrowForward:    "\ue5c8"
    readonly property string iconChevronLeft:     "\ue5cb"
    readonly property string iconChevronRight:    "\ue5cc"
    readonly property string iconKeyboardArrowDown: "\ue313"
    readonly property string iconKeyboardArrowUp:   "\ue316"

    // Actions / Controls
    readonly property string iconCheck:           "\ue668"
    readonly property string iconClose:           "\ue5cd"
    readonly property string iconCancel:          "\ue888"
    readonly property string iconCheckCircleOutline: "\uf0be"
    readonly property string iconRefresh:         "\ue5d5"
    readonly property string iconPowerSettingsNew: "\uf8c7"
    readonly property string iconLock:            "\ue899"

    // Status / Info
    readonly property string iconInfoOutline:     "\ue88e"
    readonly property string iconErrorOutline:    "\uf8b6"
    readonly property string iconWarningAmber:    "\uf083"
    readonly property string iconSnowflake:       "\ueb3b"
    readonly property string iconSevereCold:      "\uebd3"
    readonly property string iconHeat:            "\uef55"
    readonly property string iconBatteryFull:     "\ue1a5"
    readonly property string iconBatteryAlert:    "\ue19c"
    readonly property string iconAutorenew:       "\ue863"
    readonly property string iconSpeed:           "\ue9e4"
    readonly property string iconTimer:           "\ue425"
    readonly property string iconSchedule:        "\uefd6"
    readonly property string iconFlag:            "\uf0c6"
    readonly property string iconLinkOff:         "\ue16f"
    readonly property string iconUsb:             "\ue1e0"

    // Theme / Display
    readonly property string iconLightMode:       "\ue518"
    readonly property string iconDarkMode:        "\ue51c"
    readonly property string iconContrast:        "\ueb37"
    readonly property string iconMap:             "\ue55b"

    // Updates
    readonly property string iconUpdate:          "\ue923"
    readonly property string iconCloudDownload:   "\ue2c0"
    readonly property string iconPhotoCamera:     "\ue412"

    // Misc
    readonly property string iconBugReport:       "\ue868"
}

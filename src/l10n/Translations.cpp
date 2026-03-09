#include "Translations.h"

Translations::Translations(QObject *parent)
    : QObject(parent)
{
    initStrings();
}

void Translations::setLanguage(const QString &lang)
{
    if (lang != m_language && (lang == QLatin1String("en") || lang == QLatin1String("de"))) {
        m_language = lang;
        emit languageChanged();
    }
}

QString Translations::lookup(const char *key) const
{
    auto langIt = m_strings.find(m_language);
    if (langIt != m_strings.end()) {
        auto it = langIt->find(QLatin1String(key));
        if (it != langIt->end())
            return *it;
    }
    auto enIt = m_strings.find(QStringLiteral("en"));
    if (enIt != m_strings.end()) {
        auto it = enIt->find(QLatin1String(key));
        if (it != enIt->end())
            return *it;
    }
    return QLatin1String(key);
}

void Translations::initStrings()
{
    auto &en = m_strings[QStringLiteral("en")];
    auto &de = m_strings[QStringLiteral("de")];

    en[QStringLiteral("menuTitle")] = QStringLiteral("MENU");
    de[QStringLiteral("menuTitle")] = QStringLiteral("MEN\u00DC");

    en[QStringLiteral("menuSettings")] = QStringLiteral("Settings");
    de[QStringLiteral("menuSettings")] = QStringLiteral("Einstellungen");

    en[QStringLiteral("menuTheme")] = QStringLiteral("Theme");
    de[QStringLiteral("menuTheme")] = QStringLiteral("Design");

    en[QStringLiteral("menuThemeAuto")] = QStringLiteral("Automatic");
    de[QStringLiteral("menuThemeAuto")] = QStringLiteral("Automatisch");

    en[QStringLiteral("menuThemeDark")] = QStringLiteral("Dark");
    de[QStringLiteral("menuThemeDark")] = QStringLiteral("Dunkel");

    en[QStringLiteral("menuThemeLight")] = QStringLiteral("Light");
    de[QStringLiteral("menuThemeLight")] = QStringLiteral("Hell");

    en[QStringLiteral("menuLanguage")] = QStringLiteral("Language");
    de[QStringLiteral("menuLanguage")] = QStringLiteral("Sprache");

    en[QStringLiteral("menuStatusBar")] = QStringLiteral("Status Bar");
    de[QStringLiteral("menuStatusBar")] = QStringLiteral("Statusleiste");

    en[QStringLiteral("menuBatteryDisplay")] = QStringLiteral("Battery Display");
    de[QStringLiteral("menuBatteryDisplay")] = QStringLiteral("Batterieanzeige");

    en[QStringLiteral("menuBatteryPercentage")] = QStringLiteral("Percentage");
    de[QStringLiteral("menuBatteryPercentage")] = QStringLiteral("Prozent");

    en[QStringLiteral("menuBatteryRange")] = QStringLiteral("Range (km)");
    de[QStringLiteral("menuBatteryRange")] = QStringLiteral("Reichweite (km)");

    en[QStringLiteral("menuGpsIcon")] = QStringLiteral("GPS Icon");
    de[QStringLiteral("menuGpsIcon")] = QStringLiteral("GPS-Symbol");

    en[QStringLiteral("menuBluetoothIcon")] = QStringLiteral("Bluetooth Icon");
    de[QStringLiteral("menuBluetoothIcon")] = QStringLiteral("Bluetooth-Symbol");

    en[QStringLiteral("menuCloudIcon")] = QStringLiteral("Cloud Icon");
    de[QStringLiteral("menuCloudIcon")] = QStringLiteral("Cloud-Symbol");

    en[QStringLiteral("menuInternetIcon")] = QStringLiteral("Internet Icon");
    de[QStringLiteral("menuInternetIcon")] = QStringLiteral("Internet-Symbol");

    en[QStringLiteral("menuClock")] = QStringLiteral("Clock");
    de[QStringLiteral("menuClock")] = QStringLiteral("Uhr");

    en[QStringLiteral("menuBlinkerStyle")] = QStringLiteral("Blinker Style");
    de[QStringLiteral("menuBlinkerStyle")] = QStringLiteral("Blinker-Stil");

    en[QStringLiteral("menuBlinkerIcon")] = QStringLiteral("Icon (default)");
    de[QStringLiteral("menuBlinkerIcon")] = QStringLiteral("Icon (Standard)");

    en[QStringLiteral("menuBlinkerOverlay")] = QStringLiteral("Fullscreen Arrow");
    de[QStringLiteral("menuBlinkerOverlay")] = QStringLiteral("Vollbild-Pfeil");

    en[QStringLiteral("menuBatteryMode")] = QStringLiteral("Battery Mode");
    de[QStringLiteral("menuBatteryMode")] = QStringLiteral("Batteriemodus");

    en[QStringLiteral("menuBatterySingle")] = QStringLiteral("Single Battery");
    de[QStringLiteral("menuBatterySingle")] = QStringLiteral("Einzelbatterie");

    en[QStringLiteral("menuBatteryDual")] = QStringLiteral("Dual Battery");
    de[QStringLiteral("menuBatteryDual")] = QStringLiteral("Dualbatterie");

    en[QStringLiteral("menuAlarm")] = QStringLiteral("Alarm");
    de[QStringLiteral("menuAlarm")] = QStringLiteral("Alarm");

    en[QStringLiteral("menuAlarmEnable")] = QStringLiteral("Enable on Lock");
    de[QStringLiteral("menuAlarmEnable")] = QStringLiteral("Bei Sperre aktivieren");

    en[QStringLiteral("menuAlarmHonk")] = QStringLiteral("Honk on Trigger");
    de[QStringLiteral("menuAlarmHonk")] = QStringLiteral("Hupen bei Ausl\u00F6sung");

    en[QStringLiteral("menuAlarmDuration")] = QStringLiteral("Duration");
    de[QStringLiteral("menuAlarmDuration")] = QStringLiteral("Dauer");

    en[QStringLiteral("menuAlarmDuration10")] = QStringLiteral("Short (10s)");
    de[QStringLiteral("menuAlarmDuration10")] = QStringLiteral("Kurz (10s)");

    en[QStringLiteral("menuAlarmDuration20")] = QStringLiteral("Medium (20s)");
    de[QStringLiteral("menuAlarmDuration20")] = QStringLiteral("Mittel (20s)");

    en[QStringLiteral("menuAlarmDuration30")] = QStringLiteral("Long (30s)");
    de[QStringLiteral("menuAlarmDuration30")] = QStringLiteral("Lang (30s)");

    en[QStringLiteral("menuSystem")] = QStringLiteral("System");
    de[QStringLiteral("menuSystem")] = QStringLiteral("System");

    en[QStringLiteral("menuEnterUms")] = QStringLiteral("Enter UMS mode");
    de[QStringLiteral("menuEnterUms")] = QStringLiteral("UMS-Modus starten");

    en[QStringLiteral("menuResetTrip")] = QStringLiteral("Reset Trip Statistics");
    de[QStringLiteral("menuResetTrip")] = QStringLiteral("Reisestatistik zur\u00FCcksetzen");

    en[QStringLiteral("menuAbout")] = QStringLiteral("About & Licenses");
    de[QStringLiteral("menuAbout")] = QStringLiteral("\u00DCber & Lizenzen");

    en[QStringLiteral("menuExit")] = QStringLiteral("Exit Menu");
    de[QStringLiteral("menuExit")] = QStringLiteral("Men\u00FC schlie\u00DFen");

    en[QStringLiteral("menuMapNav")] = QStringLiteral("Map & Navigation");
    de[QStringLiteral("menuMapNav")] = QStringLiteral("Karte & Navigation");

    en[QStringLiteral("menuRenderMode")] = QStringLiteral("Rendering Mode");
    de[QStringLiteral("menuRenderMode")] = QStringLiteral("Darstellungsmodus");

    en[QStringLiteral("menuVector")] = QStringLiteral("Vector");
    de[QStringLiteral("menuVector")] = QStringLiteral("Vektor");

    en[QStringLiteral("menuRaster")] = QStringLiteral("Raster");
    de[QStringLiteral("menuRaster")] = QStringLiteral("Raster");

    en[QStringLiteral("menuMapType")] = QStringLiteral("Map Type");
    de[QStringLiteral("menuMapType")] = QStringLiteral("Kartentyp");

    en[QStringLiteral("menuOnline")] = QStringLiteral("Online");
    de[QStringLiteral("menuOnline")] = QStringLiteral("Online");

    en[QStringLiteral("menuOffline")] = QStringLiteral("Offline");
    de[QStringLiteral("menuOffline")] = QStringLiteral("Offline");

    en[QStringLiteral("menuNavRouting")] = QStringLiteral("Navigation Routing");
    de[QStringLiteral("menuNavRouting")] = QStringLiteral("Navigations-Routing");

    en[QStringLiteral("menuOnlineOsm")] = QStringLiteral("Online (OpenStreetMap)");
    de[QStringLiteral("menuOnlineOsm")] = QStringLiteral("Online (OpenStreetMap)");

    // Visibility options
    en[QStringLiteral("optAlways")] = QStringLiteral("Always");
    de[QStringLiteral("optAlways")] = QStringLiteral("Immer");

    en[QStringLiteral("optActiveOrError")] = QStringLiteral("Active or Error");
    de[QStringLiteral("optActiveOrError")] = QStringLiteral("Aktiv oder Fehler");

    en[QStringLiteral("optErrorOnly")] = QStringLiteral("Error Only");
    de[QStringLiteral("optErrorOnly")] = QStringLiteral("Nur Fehler");

    en[QStringLiteral("optNever")] = QStringLiteral("Never");
    de[QStringLiteral("optNever")] = QStringLiteral("Nie");

    // Control hints
    en[QStringLiteral("controlBack")] = QStringLiteral("Back");
    de[QStringLiteral("controlBack")] = QStringLiteral("Zur\u00FCck");

    en[QStringLiteral("controlExit")] = QStringLiteral("Exit");
    de[QStringLiteral("controlExit")] = QStringLiteral("Schlie\u00DFen");

    en[QStringLiteral("controlSelect")] = QStringLiteral("Select");
    de[QStringLiteral("controlSelect")] = QStringLiteral("Ausw\u00E4hlen");

    en[QStringLiteral("controlNext")] = QStringLiteral("Next Item");
    de[QStringLiteral("controlNext")] = QStringLiteral("N\u00E4chster Eintrag");

    en[QStringLiteral("controlScroll")] = QStringLiteral("Scroll");
    de[QStringLiteral("controlScroll")] = QStringLiteral("Scrollen");

    // M6: Shutdown / overlays
    en[QStringLiteral("shuttingDown")] = QStringLiteral("Shutting down...");
    de[QStringLiteral("shuttingDown")] = QStringLiteral("Herunterfahren...");

    en[QStringLiteral("connectingTitle")] = QStringLiteral("Trying to connect to vehicle system...");
    de[QStringLiteral("connectingTitle")] = QStringLiteral("Verbindung zum Fahrzeugsystem wird hergestellt...");

    en[QStringLiteral("connectingExplanation")] = QStringLiteral("This usually indicates a missing or unreliable connection between the dashboard computer (DBC) and the middle driver board (MDB). Check the USB cable if this persists.");
    de[QStringLiteral("connectingExplanation")] = QStringLiteral("Dies deutet normalerweise auf eine fehlende oder unzuverl\u00E4ssige Verbindung zwischen dem Dashboard-Computer (DBC) und dem mittleren Treiberboard (MDB) hin. Pr\u00FCfen Sie das USB-Kabel, wenn das Problem weiterhin besteht.");

    en[QStringLiteral("connectingBypassHint")] = QStringLiteral("To put your scooter into drive mode anyway, raise the kickstand, hold both brakes and press the seatbox button.");
    de[QStringLiteral("connectingBypassHint")] = QStringLiteral("Um den Roller trotzdem in den Fahrmodus zu versetzen, klappen Sie den St\u00E4nder hoch, halten Sie beide Bremsen und dr\u00FCcken Sie die Sitzbanktaste.");

    en[QStringLiteral("umsPreparing")] = QStringLiteral("Preparing Storage");
    de[QStringLiteral("umsPreparing")] = QStringLiteral("Speicher wird vorbereitet");

    en[QStringLiteral("umsActive")] = QStringLiteral("USB Mass Storage");
    de[QStringLiteral("umsActive")] = QStringLiteral("USB-Massenspeicher");

    en[QStringLiteral("umsConnect")] = QStringLiteral("Connect to Computer");
    de[QStringLiteral("umsConnect")] = QStringLiteral("Mit Computer verbinden");

    en[QStringLiteral("umsProcessing")] = QStringLiteral("Processing Files");
    de[QStringLiteral("umsProcessing")] = QStringLiteral("Dateien werden verarbeitet");

    en[QStringLiteral("blePinPrompt")] = QStringLiteral("Enter the PIN on your device");
    de[QStringLiteral("blePinPrompt")] = QStringLiteral("Geben Sie die PIN auf Ihrem Ger\u00E4t ein");

    en[QStringLiteral("hibernatePrompt")] = QStringLiteral("Hold Both Brakes to Hibernate");
    de[QStringLiteral("hibernatePrompt")] = QStringLiteral("Beide Bremsen halten zum Ruhezustand");

    en[QStringLiteral("hibernateTapKeycard")] = QStringLiteral("Or Tap Keycard");
    de[QStringLiteral("hibernateTapKeycard")] = QStringLiteral("Oder Schl\u00FCsselkarte antippen");

    en[QStringLiteral("hibernateSeatboxOpen")] = QStringLiteral("Seatbox Open");
    de[QStringLiteral("hibernateSeatboxOpen")] = QStringLiteral("Sitzbank offen");

    en[QStringLiteral("hibernateCloseSeatbox")] = QStringLiteral("Close Seatbox");
    de[QStringLiteral("hibernateCloseSeatbox")] = QStringLiteral("Sitzbank schlie\u00DFen");

    en[QStringLiteral("hibernating")] = QStringLiteral("Hibernating...");
    de[QStringLiteral("hibernating")] = QStringLiteral("Ruhezustand...");

    en[QStringLiteral("aboutTitle")] = QStringLiteral("LibreScoot");
    de[QStringLiteral("aboutTitle")] = QStringLiteral("LibreScoot");

    en[QStringLiteral("nonCommercialLicense")] = QStringLiteral("This software is licensed for non-commercial use only.");
    de[QStringLiteral("nonCommercialLicense")] = QStringLiteral("Diese Software ist nur f\u00FCr nicht-kommerzielle Nutzung lizenziert.");

    // M7: Navigation
    en[QStringLiteral("navCalculating")] = QStringLiteral("Calculating route...");
    de[QStringLiteral("navCalculating")] = QStringLiteral("Route wird berechnet...");

    en[QStringLiteral("navRecalculating")] = QStringLiteral("Recalculating...");
    de[QStringLiteral("navRecalculating")] = QStringLiteral("Neuberechnung...");

    en[QStringLiteral("navArrived")] = QStringLiteral("You have arrived");
    de[QStringLiteral("navArrived")] = QStringLiteral("Sie sind angekommen");

    en[QStringLiteral("navOffRoute")] = QStringLiteral("Off route");
    de[QStringLiteral("navOffRoute")] = QStringLiteral("Von Route abgewichen");

    en[QStringLiteral("navSetDestination")] = QStringLiteral("Set a destination to start navigation");
    de[QStringLiteral("navSetDestination")] = QStringLiteral("Ziel festlegen, um die Navigation zu starten");

    en[QStringLiteral("navUnavailable")] = QStringLiteral("Navigation unavailable");
    de[QStringLiteral("navUnavailable")] = QStringLiteral("Navigation nicht verf\u00FCgbar");

    en[QStringLiteral("navEnterCode")] = QStringLiteral("ENTER DESTINATION CODE");
    de[QStringLiteral("navEnterCode")] = QStringLiteral("ZIELCODE EINGEBEN");

    en[QStringLiteral("navConfirmDest")] = QStringLiteral("CONFIRM DESTINATION");
    de[QStringLiteral("navConfirmDest")] = QStringLiteral("ZIEL BEST\u00C4TIGEN");

    en[QStringLiteral("navThen")] = QStringLiteral("Then");
    de[QStringLiteral("navThen")] = QStringLiteral("Dann");
}

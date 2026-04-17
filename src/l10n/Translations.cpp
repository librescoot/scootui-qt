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

    // -----------------------------------------------------------------------
    // Menu strings
    // -----------------------------------------------------------------------

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

    en[QStringLiteral("menuEnterUms")] = QStringLiteral("Enter Update mode");
    de[QStringLiteral("menuEnterUms")] = QStringLiteral("Update-Modus starten");

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

    en[QStringLiteral("menuMapUpdateCheck")] = QStringLiteral("Map Update Check");
    de[QStringLiteral("menuMapUpdateCheck")] = QStringLiteral("Kartenupdate-Pr\u00fcfung");

    en[QStringLiteral("menuMapAutoDownload")] = QStringLiteral("Auto-download Maps");
    de[QStringLiteral("menuMapAutoDownload")] = QStringLiteral("Karten auto. laden");

    en[QStringLiteral("menuOnlineOsm")] = QStringLiteral("Online (OpenStreetMap)");
    de[QStringLiteral("menuOnlineOsm")] = QStringLiteral("Online (OpenStreetMap)");

    // -----------------------------------------------------------------------
    // Menu headers
    // -----------------------------------------------------------------------

    en[QStringLiteral("menuNavigationHeader")] = QStringLiteral("NAVIGATION");
    de[QStringLiteral("menuNavigationHeader")] = QStringLiteral("NAVIGATION");

    en[QStringLiteral("menuSettingsHeader")] = QStringLiteral("SETTINGS");
    de[QStringLiteral("menuSettingsHeader")] = QStringLiteral("EINSTELLUNGEN");

    en[QStringLiteral("menuThemeHeader")] = QStringLiteral("CHANGE THEME");
    de[QStringLiteral("menuThemeHeader")] = QStringLiteral("DESIGN \u00C4NDERN");

    en[QStringLiteral("menuLanguageHeader")] = QStringLiteral("LANGUAGE");
    de[QStringLiteral("menuLanguageHeader")] = QStringLiteral("SPRACHE");

    en[QStringLiteral("menuAlarmHeader")] = QStringLiteral("ALARM");
    de[QStringLiteral("menuAlarmHeader")] = QStringLiteral("ALARM");

    en[QStringLiteral("menuSavedLocationsHeader")] = QStringLiteral("SAVED LOCATIONS");
    de[QStringLiteral("menuSavedLocationsHeader")] = QStringLiteral("GESPEICHERTE ORTE");

    en[QStringLiteral("menuAlarmDurationHeader")] = QStringLiteral("ALARM DURATION");
    de[QStringLiteral("menuAlarmDurationHeader")] = QStringLiteral("ALARMDAUER");

    // -----------------------------------------------------------------------
    // Menu actions
    // -----------------------------------------------------------------------

    en[QStringLiteral("menuToggleHazardLights")] = QStringLiteral("Toggle Hazard Lights");
    de[QStringLiteral("menuToggleHazardLights")] = QStringLiteral("Warnblinker umschalten");

    en[QStringLiteral("menuHopOn")] = QStringLiteral("Hop On / Off");
    de[QStringLiteral("menuHopOn")] = QStringLiteral("Hop-on / -off");

    en[QStringLiteral("menuHopOnHeader")] = QStringLiteral("HOP ON");
    de[QStringLiteral("menuHopOnHeader")] = QStringLiteral("HOP ON");

    en[QStringLiteral("menuHopOnActivate")] = QStringLiteral("Activate now");
    de[QStringLiteral("menuHopOnActivate")] = QStringLiteral("Jetzt aktivieren");

    en[QStringLiteral("menuHopOnActivateTop")] = QStringLiteral("Activate Hop-on");
    de[QStringLiteral("menuHopOnActivateTop")] = QStringLiteral("Hop-on aktivieren");

    en[QStringLiteral("menuHopOnRelearn")] = QStringLiteral("Set new combo");
    de[QStringLiteral("menuHopOnRelearn")] = QStringLiteral("Neue Kombi lernen");

    en[QStringLiteral("menuHopOnDisable")] = QStringLiteral("Disable hop-on");
    de[QStringLiteral("menuHopOnDisable")] = QStringLiteral("Hop-on deaktivieren");

    en[QStringLiteral("hopOnLearnTitle")] = QStringLiteral("Press your sequence");
    de[QStringLiteral("hopOnLearnTitle")] = QStringLiteral("Drücke deine Sequenz");

    en[QStringLiteral("hopOnLearnHint")] = QStringLiteral("Saves 5 s after the last press. Touch nothing to abort.");
    de[QStringLiteral("hopOnLearnHint")] = QStringLiteral("Speichert 5 s nach dem letzten Druck. Nichts berühren = abbrechen.");

    en[QStringLiteral("hopOnLockedTitle")] = QStringLiteral("Hop-on active");
    de[QStringLiteral("hopOnLockedTitle")] = QStringLiteral("Hop-on aktiv");

    en[QStringLiteral("hopOnLockedHint")] = QStringLiteral("Press your combo to unlock");
    de[QStringLiteral("hopOnLockedHint")] = QStringLiteral("Drücke deine Kombi zum Entsperren");

    en[QStringLiteral("hopOnSavedToast")] = QStringLiteral("Combo saved");
    de[QStringLiteral("hopOnSavedToast")] = QStringLiteral("Kombi gespeichert");

    en[QStringLiteral("hopOnAbortedToast")] = QStringLiteral("Aborted (need at least 2 inputs)");
    de[QStringLiteral("hopOnAbortedToast")] = QStringLiteral("Abgebrochen (mindestens 2 Eingaben nötig)");

    en[QStringLiteral("menuSwitchToCluster")] = QStringLiteral("Switch to Cluster View");
    de[QStringLiteral("menuSwitchToCluster")] = QStringLiteral("Zur Tacho-Ansicht");

    en[QStringLiteral("menuSwitchToMap")] = QStringLiteral("Switch to Map View");
    de[QStringLiteral("menuSwitchToMap")] = QStringLiteral("Zur Kartenansicht");

    en[QStringLiteral("menuEnterDestinationCode")] = QStringLiteral("Enter Address");
    de[QStringLiteral("menuEnterDestinationCode")] = QStringLiteral("Adresse eingeben");

    en[QStringLiteral("menuDeleteLocation")] = QStringLiteral("Delete Location");
    de[QStringLiteral("menuDeleteLocation")] = QStringLiteral("Ort l\u00F6schen");

    en[QStringLiteral("menuStartNavigation")] = QStringLiteral("Start Navigation");
    de[QStringLiteral("menuStartNavigation")] = QStringLiteral("Navigation starten");

    en[QStringLiteral("menuStopNavigation")] = QStringLiteral("Stop Navigation");
    de[QStringLiteral("menuStopNavigation")] = QStringLiteral("Navigation beenden");

    // -----------------------------------------------------------------------
    // Visibility options
    // -----------------------------------------------------------------------

    en[QStringLiteral("optAlways")] = QStringLiteral("Always");
    de[QStringLiteral("optAlways")] = QStringLiteral("Immer");

    en[QStringLiteral("optActiveOrError")] = QStringLiteral("Active or Error");
    de[QStringLiteral("optActiveOrError")] = QStringLiteral("Aktiv oder Fehler");

    en[QStringLiteral("optErrorOnly")] = QStringLiteral("Error Only");
    de[QStringLiteral("optErrorOnly")] = QStringLiteral("Nur Fehler");

    en[QStringLiteral("optNever")] = QStringLiteral("Never");
    de[QStringLiteral("optNever")] = QStringLiteral("Nie");

    // -----------------------------------------------------------------------
    // Control hints
    // -----------------------------------------------------------------------

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

    en[QStringLiteral("controlCancel")] = QStringLiteral("Cancel");
    de[QStringLiteral("controlCancel")] = QStringLiteral("Abbrechen");

    en[QStringLiteral("navGo")] = QStringLiteral("Go!");
    de[QStringLiteral("navGo")] = QStringLiteral("Los!");

    en[QStringLiteral("controlLeftBrake")] = QStringLiteral("Left Brake");
    de[QStringLiteral("controlLeftBrake")] = QStringLiteral("Linke Bremse");

    en[QStringLiteral("controlRightBrake")] = QStringLiteral("Right Brake");
    de[QStringLiteral("controlRightBrake")] = QStringLiteral("Rechte Bremse");

    en[QStringLiteral("controlNextItem")] = QStringLiteral("Next Item");
    de[QStringLiteral("controlNextItem")] = QStringLiteral("N\u00E4chster Eintrag");

    en[QStringLiteral("controlPressRightBrakeConfirm")] = QStringLiteral("Press Right Brake to Confirm");
    de[QStringLiteral("controlPressRightBrakeConfirm")] = QStringLiteral("Rechte Bremse zum Best\u00E4tigen");

    en[QStringLiteral("controlPressLeftBrakeEdit")] = QStringLiteral("Press Left Brake to Edit");
    de[QStringLiteral("controlPressLeftBrakeEdit")] = QStringLiteral("Linke Bremse zum Bearbeiten");

    // -----------------------------------------------------------------------
    // Shutdown states
    // -----------------------------------------------------------------------

    en[QStringLiteral("shuttingDown")] = QStringLiteral("Shutting down...");
    de[QStringLiteral("shuttingDown")] = QStringLiteral("Wird heruntergefahren...");

    en[QStringLiteral("shutdownComplete")] = QStringLiteral("Shutdown complete.\nTap keycard to unlock.");
    de[QStringLiteral("shutdownComplete")] = QStringLiteral("Herunterfahren abgeschlossen.\nKeycard antippen zum Entsperren.");

    en[QStringLiteral("shutdownSuspending")] = QStringLiteral("Suspending...");
    de[QStringLiteral("shutdownSuspending")] = QStringLiteral("Wird pausiert...");

    en[QStringLiteral("shutdownHibernationImminent")] = QStringLiteral("Hibernation imminent...");
    de[QStringLiteral("shutdownHibernationImminent")] = QStringLiteral("Ruhezustand steht bevor...");

    en[QStringLiteral("shutdownSuspensionImminent")] = QStringLiteral("Suspension imminent...");
    de[QStringLiteral("shutdownSuspensionImminent")] = QStringLiteral("Standby steht bevor...");

    en[QStringLiteral("shutdownProcessing")] = QStringLiteral("Processing...");
    de[QStringLiteral("shutdownProcessing")] = QStringLiteral("Verarbeitung...");

    // -----------------------------------------------------------------------
    // Connection
    // -----------------------------------------------------------------------

    en[QStringLiteral("connectingTitle")] = QStringLiteral("Trying to connect to vehicle system...");
    de[QStringLiteral("connectingTitle")] = QStringLiteral("Verbindung zum Fahrzeugsystem wird hergestellt...");

    en[QStringLiteral("connectingExplanation")] = QStringLiteral("This usually indicates a missing or unreliable connection between the dashboard computer (DBC) and the middle driver board (MDB). Check the USB cable if this persists.");
    de[QStringLiteral("connectingExplanation")] = QStringLiteral("Dies deutet normalerweise auf eine fehlende oder unzuverl\u00E4ssige Verbindung zwischen dem Dashboard-Computer (DBC) und dem mittleren Treiberboard (MDB) hin. Pr\u00FCfen Sie das USB-Kabel, wenn das Problem weiterhin besteht.");

    en[QStringLiteral("connectingBypassHint")] = QStringLiteral("To put your scooter into drive mode anyway, raise the kickstand, hold both brakes and press the seatbox button.");
    de[QStringLiteral("connectingBypassHint")] = QStringLiteral("Um den Roller trotzdem in den Fahrmodus zu versetzen, klappen Sie den St\u00E4nder hoch, halten Sie beide Bremsen und dr\u00FCcken Sie die Sitzbanktaste.");

    en[QStringLiteral("connectionLost")] = QStringLiteral("Connection to vehicle system lost");
    de[QStringLiteral("connectionLost")] = QStringLiteral("Verbindung zum Fahrzeugsystem verloren");

    en[QStringLiteral("connectionReconnecting")] = QStringLiteral("Attempting to reconnect to vehicle system...");
    de[QStringLiteral("connectionReconnecting")] = QStringLiteral("Verbindung zum Fahrzeugsystem wird wiederhergestellt...");

    en[QStringLiteral("connectionRestored")] = QStringLiteral("Connected to vehicle system");
    de[QStringLiteral("connectionRestored")] = QStringLiteral("Fahrzeugsystem verbunden");

    // -----------------------------------------------------------------------
    // UMS
    // -----------------------------------------------------------------------

    en[QStringLiteral("umsPreparing")] = QStringLiteral("Preparing Storage");
    de[QStringLiteral("umsPreparing")] = QStringLiteral("Speicher wird vorbereitet");

    en[QStringLiteral("umsActive")] = QStringLiteral("Update Mode");
    de[QStringLiteral("umsActive")] = QStringLiteral("Update-Modus");

    en[QStringLiteral("umsConnect")] = QStringLiteral("Connect to Computer");
    de[QStringLiteral("umsConnect")] = QStringLiteral("Mit Computer verbinden");

    en[QStringLiteral("umsProcessing")] = QStringLiteral("Processing Files");
    de[QStringLiteral("umsProcessing")] = QStringLiteral("Dateien werden verarbeitet");

    en[QStringLiteral("umsTitle")] = QStringLiteral("Update Mode");
    de[QStringLiteral("umsTitle")] = QStringLiteral("Update-Modus");

    en[QStringLiteral("umsConnectToComputer")] = QStringLiteral("Connect to a computer to transfer files.");
    de[QStringLiteral("umsConnectToComputer")] = QStringLiteral("Zum Dateitransfer mit Computer verbinden.");

    en[QStringLiteral("umsHoldExit")] = QStringLiteral("Exit");
    de[QStringLiteral("umsHoldExit")] = QStringLiteral("Beenden");

    en[QStringLiteral("controlLeftBrakeHold")] = QStringLiteral("Left Brake (Hold)");
    de[QStringLiteral("controlLeftBrakeHold")] = QStringLiteral("Linke Bremse (Halten)");

    // -----------------------------------------------------------------------
    // Bluetooth
    // -----------------------------------------------------------------------

    en[QStringLiteral("blePinPrompt")] = QStringLiteral("Enter the PIN on your device");
    de[QStringLiteral("blePinPrompt")] = QStringLiteral("Geben Sie die PIN auf Ihrem Ger\u00E4t ein");

    en[QStringLiteral("bleCommError")] = QStringLiteral("BLE: Communication error");
    de[QStringLiteral("bleCommError")] = QStringLiteral("BLE: Kommunikationsfehler");

    en[QStringLiteral("bluetoothCommError")] = QStringLiteral("Bluetooth service communication error");
    de[QStringLiteral("bluetoothCommError")] = QStringLiteral("Bluetooth-Kommunikationsfehler");

    en[QStringLiteral("bluetoothPinInstruction")] = QStringLiteral("Use this code to pair your device");
    de[QStringLiteral("bluetoothPinInstruction")] = QStringLiteral("Diesen Code zum Koppeln verwenden");

    // -----------------------------------------------------------------------
    // Hibernation
    // -----------------------------------------------------------------------

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

    en[QStringLiteral("hibernationTitle")] = QStringLiteral("Manual Hibernation");
    de[QStringLiteral("hibernationTitle")] = QStringLiteral("Manueller Ruhezustand");

    en[QStringLiteral("hibernationTapKeycardToConfirm")] = QStringLiteral("Tap keycard to confirm");
    de[QStringLiteral("hibernationTapKeycardToConfirm")] = QStringLiteral("Keycard antippen zum Best\u00E4tigen");

    en[QStringLiteral("hibernationKeepHoldingBrakes")] = QStringLiteral("Keep holding brakes to force");
    de[QStringLiteral("hibernationKeepHoldingBrakes")] = QStringLiteral("Bremsen weiterhin halten zum Erzwingen");

    en[QStringLiteral("hibernationOrHoldBrakes")] = QStringLiteral("Or hold both brakes for 15s to force");
    de[QStringLiteral("hibernationOrHoldBrakes")] = QStringLiteral("Oder beide Bremsen 15s halten zum Erzwingen");

    en[QStringLiteral("hibernationCancel")] = QStringLiteral("CANCEL");
    de[QStringLiteral("hibernationCancel")] = QStringLiteral("ABBRECHEN");

    en[QStringLiteral("hibernationKickstand")] = QStringLiteral("Kickstand");
    de[QStringLiteral("hibernationKickstand")] = QStringLiteral("Seitenst\u00E4nder");

    en[QStringLiteral("hibernationConfirm")] = QStringLiteral("CONFIRM");
    de[QStringLiteral("hibernationConfirm")] = QStringLiteral("BEST\u00C4TIGEN");

    // -----------------------------------------------------------------------
    // Auto-lock countdown
    // -----------------------------------------------------------------------

    en[QStringLiteral("autoLockTitle")] = QStringLiteral("Auto-Locking");
    de[QStringLiteral("autoLockTitle")] = QStringLiteral("Automatisches Sperren");

    en[QStringLiteral("autoLockCancelHint")] = QStringLiteral("Touch a brake or kickstand to cancel");
    de[QStringLiteral("autoLockCancelHint")] = QStringLiteral("Bremse oder Seitenst\u00E4nder bet\u00E4tigen zum Abbrechen");

    // -----------------------------------------------------------------------
    // About
    // -----------------------------------------------------------------------

    en[QStringLiteral("aboutTitle")] = QStringLiteral("LibreScoot");
    de[QStringLiteral("aboutTitle")] = QStringLiteral("LibreScoot");

    en[QStringLiteral("aboutNonCommercialTitle")] = QStringLiteral("NON-COMMERCIAL SOFTWARE");
    de[QStringLiteral("aboutNonCommercialTitle")] = QStringLiteral("NICHT-KOMMERZIELLE SOFTWARE");

    en[QStringLiteral("aboutFossDescription")] = QStringLiteral("FOSS firmware for unu Scooter Pro e-mopeds");
    de[QStringLiteral("aboutFossDescription")] = QStringLiteral("FOSS-Firmware f\u00FCr unu Scooter Pro E-Mopeds");

    en[QStringLiteral("aboutOpenSourceComponents")] = QStringLiteral("OPEN SOURCE COMPONENTS");
    de[QStringLiteral("aboutOpenSourceComponents")] = QStringLiteral("OPEN-SOURCE-KOMPONENTEN");

    en[QStringLiteral("aboutScrollAction")] = QStringLiteral("Scroll");
    de[QStringLiteral("aboutScrollAction")] = QStringLiteral("Scrollen");

    en[QStringLiteral("aboutBackAction")] = QStringLiteral("Back");
    de[QStringLiteral("aboutBackAction")] = QStringLiteral("Zur\u00FCck");

    en[QStringLiteral("aboutBootThemeRestored")] = QStringLiteral("Boot theme: LibreScoot restored.");
    de[QStringLiteral("aboutBootThemeRestored")] = QStringLiteral("Boot-Theme: LibreScoot wiederhergestellt.");

    en[QStringLiteral("aboutGenuineAdvantage")] = QStringLiteral("Genuine Advantage activated.");
    de[QStringLiteral("aboutGenuineAdvantage")] = QStringLiteral("Genuine Advantage aktiviert.");

    en[QStringLiteral("nonCommercialLicense")] = QStringLiteral("This software is licensed for non-commercial use only.");
    de[QStringLiteral("nonCommercialLicense")] = QStringLiteral("Diese Software ist nur f\u00FCr nicht-kommerzielle Nutzung lizenziert.");

    en[QStringLiteral("aboutCommercialProhibited")] = QStringLiteral("Commercial distribution, resale, or preinstallation on devices for sale is prohibited under CC BY-NC-SA 4.0. Authorized installation partners, listed below, are permitted to commercially install this software.");
    de[QStringLiteral("aboutCommercialProhibited")] = QStringLiteral("Kommerzieller Vertrieb, Weiterverkauf oder Vorinstallation auf Ger\u00E4ten zum Verkauf ist unter CC BY-NC-SA 4.0 untersagt. Autorisierte Installationspartner, unten aufgef\u00FChrt, d\u00FCrfen diese Software kommerziell installieren.");

    en[QStringLiteral("aboutAuthorizedPartners")] = QStringLiteral("AUTHORIZED INSTALLATION PARTNERS");
    de[QStringLiteral("aboutAuthorizedPartners")] = QStringLiteral("AUTORISIERTE INSTALLATIONSPARTNER");

    en[QStringLiteral("aboutScamWarning")] = QStringLiteral("If you paid money for this software, or if you purchased a new scooter from a shop or vendor with this software preinstalled, you may have been the victim of a scam. Please report it at https://librescoot.org.");
    de[QStringLiteral("aboutScamWarning")] = QStringLiteral("Falls du f\u00FCr diese Software bezahlt hast, oder wenn du einen neuen Roller von einem H\u00E4ndler oder Verk\u00E4ufer mit vorinstallierter Software erworben hast, bist du m\u00F6glicherweise Opfer eines Betrugs geworden. Bitte melde es auf https://librescoot.org.");

    en[QStringLiteral("aboutSpecialThanks")] = QStringLiteral("SPECIAL THANKS TO THE EARLY TESTERS");
    de[QStringLiteral("aboutSpecialThanks")] = QStringLiteral("BESONDERER DANK AN DIE EARLY TESTER");

    en[QStringLiteral("aboutPatienceNote")] = QStringLiteral("And Cin and Tabitha for their patience with the scooters in the hallway.");
    de[QStringLiteral("aboutPatienceNote")] = QStringLiteral("Und Cin und Tabitha f\u00FCr ihre Geduld f\u00FCr die Roller im Flur.");

    // -----------------------------------------------------------------------
    // Toast / monitoring
    // -----------------------------------------------------------------------

    en[QStringLiteral("lowTempWarning")] = QStringLiteral("Low Temperatures - Ride Carefully");
    de[QStringLiteral("lowTempWarning")] = QStringLiteral("Niedrige Temperaturen - Vorsichtig fahren");

    en[QStringLiteral("lowTempMotor")] = QStringLiteral("Motor");
    de[QStringLiteral("lowTempMotor")] = QStringLiteral("Motor");

    en[QStringLiteral("lowTempBattery")] = QStringLiteral("Battery");
    de[QStringLiteral("lowTempBattery")] = QStringLiteral("Batterie");

    en[QStringLiteral("lowTemp12vBattery")] = QStringLiteral("12V Battery");
    de[QStringLiteral("lowTemp12vBattery")] = QStringLiteral("12V-Batterie");

    en[QStringLiteral("redisDisconnected")] = QStringLiteral("System connection lost");
    de[QStringLiteral("redisDisconnected")] = QStringLiteral("Systemverbindung verloren");

    en[QStringLiteral("usbDisconnected")] = QStringLiteral("USB connection interrupted");
    de[QStringLiteral("usbDisconnected")] = QStringLiteral("USB-Verbindung unterbrochen");

    en[QStringLiteral("locationSaved")] = QStringLiteral("Location saved");
    de[QStringLiteral("locationSaved")] = QStringLiteral("Standort gespeichert");

    en[QStringLiteral("locationDeleted")] = QStringLiteral("Location deleted");
    de[QStringLiteral("locationDeleted")] = QStringLiteral("Standort gel\u00F6scht");

    en[QStringLiteral("maxLocationsReached")] = QStringLiteral("Maximum saved locations reached");
    de[QStringLiteral("maxLocationsReached")] = QStringLiteral("Maximale Anzahl gespeicherter Standorte erreicht");

    en[QStringLiteral("savedLocationsFailed")] = QStringLiteral("Failed to load saved locations");
    de[QStringLiteral("savedLocationsFailed")] = QStringLiteral("Gespeicherte Orte konnten nicht geladen werden");

    en[QStringLiteral("menuSavedLocations")] = QStringLiteral("Saved Locations");
    de[QStringLiteral("menuSavedLocations")] = QStringLiteral("Gespeicherte Orte");

    en[QStringLiteral("menuSaveLocation")] = QStringLiteral("Save Current Location");
    de[QStringLiteral("menuSaveLocation")] = QStringLiteral("Aktuellen Standort speichern");

    en[QStringLiteral("menuNavSetup")] = QStringLiteral("Navigation Setup");
    de[QStringLiteral("menuNavSetup")] = QStringLiteral("Navigationseinrichtung");

    en[QStringLiteral("plymouthToggled")] = QStringLiteral("Boot theme changed");
    de[QStringLiteral("plymouthToggled")] = QStringLiteral("Startdesign ge\u00E4ndert");

    // -----------------------------------------------------------------------
    // Navigation
    // -----------------------------------------------------------------------

    en[QStringLiteral("navCalculating")] = QStringLiteral("Calculating route...");
    de[QStringLiteral("navCalculating")] = QStringLiteral("Route wird berechnet...");

    en[QStringLiteral("navRecalculating")] = QStringLiteral("Recalculating route...");
    de[QStringLiteral("navRecalculating")] = QStringLiteral("Route wird neu berechnet...");

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

    en[QStringLiteral("navYouHaveArrived")] = QStringLiteral("You have arrived!");
    de[QStringLiteral("navYouHaveArrived")] = QStringLiteral("Ziel erreicht!");

    en[QStringLiteral("navDistance")] = QStringLiteral("Distance");
    de[QStringLiteral("navDistance")] = QStringLiteral("Entfernung");

    en[QStringLiteral("navRemaining")] = QStringLiteral("Remaining");
    de[QStringLiteral("navRemaining")] = QStringLiteral("Verbleibend");

    en[QStringLiteral("navEta")] = QStringLiteral("ETA");
    de[QStringLiteral("navEta")] = QStringLiteral("Ankunft");

    en[QStringLiteral("navContinue")] = QStringLiteral("Continue");
    de[QStringLiteral("navContinue")] = QStringLiteral("Weiter");

    en[QStringLiteral("navReturnToRoute")] = QStringLiteral("Return to the route");
    de[QStringLiteral("navReturnToRoute")] = QStringLiteral("Zur\u00FCck zur Route");

    en[QStringLiteral("navCurrentPositionNotAvailable")] = QStringLiteral("Current position not available");
    de[QStringLiteral("navCurrentPositionNotAvailable")] = QStringLiteral("Aktuelle Position nicht verf\u00FCgbar");

    en[QStringLiteral("navCouldNotCalculateRoute")] = QStringLiteral("Could not calculate route");
    de[QStringLiteral("navCouldNotCalculateRoute")] = QStringLiteral("Route konnte nicht berechnet werden");

    en[QStringLiteral("navDestinationUnreachable")] = QStringLiteral("Destination is unreachable. Please select a different location.");
    de[QStringLiteral("navDestinationUnreachable")] = QStringLiteral("Ziel ist nicht erreichbar. Bitte anderen Standort w\u00E4hlen.");

    en[QStringLiteral("navNewDestination")] = QStringLiteral("New navigation destination received. Calculating route...");
    de[QStringLiteral("navNewDestination")] = QStringLiteral("Neues Navigationsziel empfangen. Route wird berechnet...");

    en[QStringLiteral("navWaitingForGps")] = QStringLiteral("Waiting for GPS fix");
    de[QStringLiteral("navWaitingForGps")] = QStringLiteral("Warte auf GPS-Signal");

    en[QStringLiteral("navWaitingForGpsRoute")] = QStringLiteral("Waiting for recent GPS fix to calculate route.");
    de[QStringLiteral("navWaitingForGpsRoute")] = QStringLiteral("Warte auf GPS-Signal zur Routenberechnung.");

    en[QStringLiteral("navResumingNavigation")] = QStringLiteral("Resuming navigation.");
    de[QStringLiteral("navResumingNavigation")] = QStringLiteral("Navigation wird fortgesetzt.");

    en[QStringLiteral("navArrivedAtDestination")] = QStringLiteral("You have arrived at your destination!");
    de[QStringLiteral("navArrivedAtDestination")] = QStringLiteral("Du hast dein Ziel erreicht!");

    en[QStringLiteral("navOffRouteRerouting")] = QStringLiteral("Off route. Attempting to reroute...");
    de[QStringLiteral("navOffRouteRerouting")] = QStringLiteral("Abseits der Route. Neue Route wird berechnet...");

    en[QStringLiteral("navCouldNotCalculateNewRoute")] = QStringLiteral("Could not calculate new route");
    de[QStringLiteral("navCouldNotCalculateNewRoute")] = QStringLiteral("Neue Route konnte nicht berechnet werden");

    en[QStringLiteral("navRouteError")] = QStringLiteral("Route error");
    de[QStringLiteral("navRouteError")] = QStringLiteral("Routenfehler");

    // -----------------------------------------------------------------------
    // Navigation short instructions
    // -----------------------------------------------------------------------

    en[QStringLiteral("navShortContinueStraight")] = QStringLiteral("continue straight");
    de[QStringLiteral("navShortContinueStraight")] = QStringLiteral("geradeaus weiter");

    en[QStringLiteral("navShortTurnLeft")] = QStringLiteral("turn left");
    de[QStringLiteral("navShortTurnLeft")] = QStringLiteral("links abbiegen");

    en[QStringLiteral("navShortTurnRight")] = QStringLiteral("turn right");
    de[QStringLiteral("navShortTurnRight")] = QStringLiteral("rechts abbiegen");

    en[QStringLiteral("navShortTurnSlightlyLeft")] = QStringLiteral("turn slightly left");
    de[QStringLiteral("navShortTurnSlightlyLeft")] = QStringLiteral("leicht links abbiegen");

    en[QStringLiteral("navShortTurnSlightlyRight")] = QStringLiteral("turn slightly right");
    de[QStringLiteral("navShortTurnSlightlyRight")] = QStringLiteral("leicht rechts abbiegen");

    en[QStringLiteral("navShortTurnSharplyLeft")] = QStringLiteral("turn sharply left");
    de[QStringLiteral("navShortTurnSharplyLeft")] = QStringLiteral("scharf links abbiegen");

    en[QStringLiteral("navShortTurnSharplyRight")] = QStringLiteral("turn sharply right");
    de[QStringLiteral("navShortTurnSharplyRight")] = QStringLiteral("scharf rechts abbiegen");

    en[QStringLiteral("navShortUturn")] = QStringLiteral("make a U-turn");
    de[QStringLiteral("navShortUturn")] = QStringLiteral("wenden");

    en[QStringLiteral("navShortUturnRight")] = QStringLiteral("make a right U-turn");
    de[QStringLiteral("navShortUturnRight")] = QStringLiteral("rechts wenden");

    en[QStringLiteral("navShortMerge")] = QStringLiteral("merge");
    de[QStringLiteral("navShortMerge")] = QStringLiteral("einf\u00E4deln");

    en[QStringLiteral("navShortMergeLeft")] = QStringLiteral("merge left");
    de[QStringLiteral("navShortMergeLeft")] = QStringLiteral("links einf\u00E4deln");

    en[QStringLiteral("navShortMergeRight")] = QStringLiteral("merge right");
    de[QStringLiteral("navShortMergeRight")] = QStringLiteral("rechts einf\u00E4deln");

    en[QStringLiteral("navShortContinue")] = QStringLiteral("continue");
    de[QStringLiteral("navShortContinue")] = QStringLiteral("weiter");

    // -----------------------------------------------------------------------
    // Navigation setup screen
    // -----------------------------------------------------------------------

    en[QStringLiteral("navSetupTitle")] = QStringLiteral("Navigation Setup");
    de[QStringLiteral("navSetupTitle")] = QStringLiteral("Navigation einrichten");

    en[QStringLiteral("navSetupTitleRoutingUnavailable")] = QStringLiteral("Routing Not Available");
    de[QStringLiteral("navSetupTitleRoutingUnavailable")] = QStringLiteral("Routing nicht verf\u00FCgbar");

    en[QStringLiteral("navSetupTitleMapsUnavailable")] = QStringLiteral("Map Tiles Not Available");
    de[QStringLiteral("navSetupTitleMapsUnavailable")] = QStringLiteral("Kartenkacheln nicht verf\u00FCgbar");

    en[QStringLiteral("navSetupTitleBothUnavailable")] = QStringLiteral("Navigation Not Available");
    de[QStringLiteral("navSetupTitleBothUnavailable")] = QStringLiteral("Navigation nicht verf\u00FCgbar");

    en[QStringLiteral("navSetupLocalDisplayMaps")] = QStringLiteral("Offline display maps");
    de[QStringLiteral("navSetupLocalDisplayMaps")] = QStringLiteral("Offline-Kartenkacheln");

    en[QStringLiteral("navSetupRoutingEngine")] = QStringLiteral("Routing engine");
    de[QStringLiteral("navSetupRoutingEngine")] = QStringLiteral("Routing-Engine");

    en[QStringLiteral("navSetupNoRoutingBody")] = QStringLiteral("Map display and routing are independent. Display tiles can be local (offline .mbtiles) or online. Routing requires a Valhalla engine \u2014 local (needs routing maps) or a remote server.");
    de[QStringLiteral("navSetupNoRoutingBody")] = QStringLiteral("Kartenanzeige und Routing sind unabh\u00E4ngig. Kartenkacheln k\u00F6nnen lokal (offline .mbtiles) oder online sein. F\u00FCr Routing wird eine Valhalla-Engine ben\u00F6tigt \u2014 lokal (Routing-Karten erforderlich) oder ein Remote-Server.");

    en[QStringLiteral("navSetupScanForInstructions")] = QStringLiteral("Scan for setup instructions");
    de[QStringLiteral("navSetupScanForInstructions")] = QStringLiteral("F\u00FCr Anleitung scannen");

    en[QStringLiteral("navSetupDisplayMapsBody")] = QStringLiteral("Download offline display maps for your region. These are used to render the map on screen without an internet connection.");
    de[QStringLiteral("navSetupDisplayMapsBody")] = QStringLiteral("Offline-Kartenkacheln f\u00FCr Ihre Region herunterladen. Diese werden verwendet, um die Karte ohne Internetverbindung anzuzeigen.");

    en[QStringLiteral("navSetupRoutingBody")] = QStringLiteral("Download routing maps for your region. These are used by the on-device Valhalla routing engine to calculate routes offline.");
    de[QStringLiteral("navSetupRoutingBody")] = QStringLiteral("Routing-Karten f\u00FCr Ihre Region herunterladen. Diese werden von der Valhalla-Routing-Engine auf dem Ger\u00E4t verwendet, um Routen offline zu berechnen.");

    en[QStringLiteral("navSetupCheckingUpdates")] = QStringLiteral("Checking for updates...");
    de[QStringLiteral("navSetupCheckingUpdates")] = QStringLiteral("Suche nach Updates...");

    en[QStringLiteral("navSetupDownloadLocating")] = QStringLiteral("Detecting your region...");
    de[QStringLiteral("navSetupDownloadLocating")] = QStringLiteral("Region wird erkannt...");

    en[QStringLiteral("navSetupDownloadInstalling")] = QStringLiteral("Installing maps...");
    de[QStringLiteral("navSetupDownloadInstalling")] = QStringLiteral("Karten werden installiert...");

    en[QStringLiteral("navSetupDownloadDone")] = QStringLiteral("Maps installed successfully");
    de[QStringLiteral("navSetupDownloadDone")] = QStringLiteral("Karten erfolgreich installiert");

    en[QStringLiteral("navSetupDownloadButton")] = QStringLiteral("Download");
    de[QStringLiteral("navSetupDownloadButton")] = QStringLiteral("Herunterladen");

    en[QStringLiteral("navSetupUpdateButton")] = QStringLiteral("Update");
    de[QStringLiteral("navSetupUpdateButton")] = QStringLiteral("Aktualisieren");

    en[QStringLiteral("navSetupResumeButton")] = QStringLiteral("Resume");
    de[QStringLiteral("navSetupResumeButton")] = QStringLiteral("Fortsetzen");

    en[QStringLiteral("navSetupDownloadError")] = QStringLiteral("Download failed");
    de[QStringLiteral("navSetupDownloadError")] = QStringLiteral("Download fehlgeschlagen");

    en[QStringLiteral("navSetupDownloadUnsupported")] = QStringLiteral("Region not supported");
    de[QStringLiteral("navSetupDownloadUnsupported")] = QStringLiteral("Region nicht unterst\u00FCtzt");

    en[QStringLiteral("navSetupInsufficientSpace")] = QStringLiteral("Insufficient disk space");
    de[QStringLiteral("navSetupInsufficientSpace")] = QStringLiteral("Nicht gen\u00FCgend Speicherplatz");

    en[QStringLiteral("navSetupDownloadNoInternet")] = QStringLiteral("No internet connection");
    de[QStringLiteral("navSetupDownloadNoInternet")] = QStringLiteral("Keine Internetverbindung");

    en[QStringLiteral("navSetupDownloadWaitingGps")] = QStringLiteral("Waiting for GPS fix...");
    de[QStringLiteral("navSetupDownloadWaitingGps")] = QStringLiteral("Warte auf GPS-Signal...");

    en[QStringLiteral("navSetupDownloadProgress")] = QStringLiteral("Downloading... %1%");
    de[QStringLiteral("navSetupDownloadProgress")] = QStringLiteral("Lade herunter... %1%");

    en[QStringLiteral("navSetupDownloadProgressBytes")] = QStringLiteral("%1 / %2 MB");
    de[QStringLiteral("navSetupDownloadProgressBytes")] = QStringLiteral("%1 / %2 MB");

    en[QStringLiteral("mapUpdateAvailableToast")] = QStringLiteral("Map update available \u2014 open Menu \u2192 Navigation Setup to install");
    de[QStringLiteral("mapUpdateAvailableToast")] = QStringLiteral("Kartenupdate verf\u00fcgbar \u2014 Men\u00fc \u2192 Navigation einrichten");

    en[QStringLiteral("mapUpdateBadge")] = QStringLiteral("Map update");
    de[QStringLiteral("mapUpdateBadge")] = QStringLiteral("Kartenupdate");

    en[QStringLiteral("menuSetupMapMode")] = QStringLiteral("Set up Map Mode");
    de[QStringLiteral("menuSetupMapMode")] = QStringLiteral("Kartenmodus einrichten");

    en[QStringLiteral("menuSetupNavigation")] = QStringLiteral("Set up Navigation");
    de[QStringLiteral("menuSetupNavigation")] = QStringLiteral("Navigation einrichten");

    // -----------------------------------------------------------------------
    // OTA
    // -----------------------------------------------------------------------

    en[QStringLiteral("otaDownloading")] = QStringLiteral("Downloading");
    de[QStringLiteral("otaDownloading")] = QStringLiteral("Lade herunter");

    en[QStringLiteral("otaInstalling")] = QStringLiteral("Installing");
    de[QStringLiteral("otaInstalling")] = QStringLiteral("Installiere");

    en[QStringLiteral("otaInitializing")] = QStringLiteral("Initializing update...");
    de[QStringLiteral("otaInitializing")] = QStringLiteral("Update wird initialisiert...");

    en[QStringLiteral("otaCheckingUpdates")] = QStringLiteral("Checking for updates...");
    de[QStringLiteral("otaCheckingUpdates")] = QStringLiteral("Suche nach Updates...");

    en[QStringLiteral("otaCheckFailed")] = QStringLiteral("Update check failed.");
    de[QStringLiteral("otaCheckFailed")] = QStringLiteral("Update-Pr\u00FCfung fehlgeschlagen.");

    en[QStringLiteral("otaDeviceUpdated")] = QStringLiteral("Device updated.");
    de[QStringLiteral("otaDeviceUpdated")] = QStringLiteral("Ger\u00E4t aktualisiert.");

    en[QStringLiteral("otaWaitingDashboard")] = QStringLiteral("Waiting for dashboard...");
    de[QStringLiteral("otaWaitingDashboard")] = QStringLiteral("Warte auf Dashboard...");

    en[QStringLiteral("otaDownloadingUpdates")] = QStringLiteral("Downloading updates...");
    de[QStringLiteral("otaDownloadingUpdates")] = QStringLiteral("Updates werden heruntergeladen...");

    en[QStringLiteral("otaDownloadFailed")] = QStringLiteral("Download failed.");
    de[QStringLiteral("otaDownloadFailed")] = QStringLiteral("Download fehlgeschlagen.");

    en[QStringLiteral("otaInstallingUpdates")] = QStringLiteral("Installing updates...");
    de[QStringLiteral("otaInstallingUpdates")] = QStringLiteral("Updates werden installiert...");

    en[QStringLiteral("otaInstallFailed")] = QStringLiteral("Installation failed.");
    de[QStringLiteral("otaInstallFailed")] = QStringLiteral("Installation fehlgeschlagen.");

    en[QStringLiteral("otaWaitingForReboot")] = QStringLiteral("Update installed. Waiting for reboot");
    de[QStringLiteral("otaWaitingForReboot")] = QStringLiteral("Update installiert. Warte auf Neustart");

    en[QStringLiteral("otaStatusWaitingForReboot")] = QStringLiteral("Waiting for reboot");
    de[QStringLiteral("otaStatusWaitingForReboot")] = QStringLiteral("Warte auf Neustart");

    en[QStringLiteral("otaStatusDownloading")] = QStringLiteral("Downloading");
    de[QStringLiteral("otaStatusDownloading")] = QStringLiteral("Wird heruntergeladen");

    en[QStringLiteral("otaStatusInstalling")] = QStringLiteral("Installing");
    de[QStringLiteral("otaStatusInstalling")] = QStringLiteral("Wird installiert");

    en[QStringLiteral("otaUpdate")] = QStringLiteral(" update");
    de[QStringLiteral("otaUpdate")] = QStringLiteral(" Update");

    en[QStringLiteral("otaInvalidRelease")] = QStringLiteral("Invalid release");
    de[QStringLiteral("otaInvalidRelease")] = QStringLiteral("Ung\u00FCltige Version");

    en[QStringLiteral("otaDownloadFailedShort")] = QStringLiteral("Download failed");
    de[QStringLiteral("otaDownloadFailedShort")] = QStringLiteral("Download fehlgeschlagen");

    en[QStringLiteral("otaInstallFailedShort")] = QStringLiteral("Install failed");
    de[QStringLiteral("otaInstallFailedShort")] = QStringLiteral("Installation fehlgeschlagen");

    en[QStringLiteral("otaRebootFailed")] = QStringLiteral("Reboot failed");
    de[QStringLiteral("otaRebootFailed")] = QStringLiteral("Neustart fehlgeschlagen");

    en[QStringLiteral("otaUpdateError")] = QStringLiteral("Update error");
    de[QStringLiteral("otaUpdateError")] = QStringLiteral("Update-Fehler");

    en[QStringLiteral("otaPreparingUpdate")] = QStringLiteral("Preparing update...");
    de[QStringLiteral("otaPreparingUpdate")] = QStringLiteral("Update wird vorbereitet...");

    en[QStringLiteral("otaPendingReboot")] = QStringLiteral("Update installed, will apply next time the scooter is started");
    de[QStringLiteral("otaPendingReboot")] = QStringLiteral("Update installiert, wird beim n\u00E4chsten Start angewendet");

    en[QStringLiteral("otaScooterWillTurnOff")] = QStringLiteral("Your scooter will turn off when done.\nYou can unlock it again at any point.");
    de[QStringLiteral("otaScooterWillTurnOff")] = QStringLiteral("Dein Roller wird danach ausgeschaltet.\nDu kannst ihn jederzeit wieder entsperren.");

    // -----------------------------------------------------------------------
    // Battery messages
    // -----------------------------------------------------------------------

    en[QStringLiteral("batteryKm")] = QStringLiteral("km");
    de[QStringLiteral("batteryKm")] = QStringLiteral("km");

    en[QStringLiteral("batteryCbNotCharging")] = QStringLiteral("CB Battery not charging");
    de[QStringLiteral("batteryCbNotCharging")] = QStringLiteral("CB-Batterie l\u00E4dt nicht");

    en[QStringLiteral("batteryAuxLowNotCharging")] = QStringLiteral("AUX Battery low and not charging");
    de[QStringLiteral("batteryAuxLowNotCharging")] = QStringLiteral("AUX-Batterie schwach und l\u00E4dt nicht");

    en[QStringLiteral("batteryAuxVoltageLow")] = QStringLiteral("AUX Battery voltage low");
    de[QStringLiteral("batteryAuxVoltageLow")] = QStringLiteral("AUX-Batterie Spannung niedrig");

    en[QStringLiteral("batteryAuxVoltageVeryLowReplace")] = QStringLiteral("AUX Battery voltage very low - may need replacement");
    de[QStringLiteral("batteryAuxVoltageVeryLowReplace")] = QStringLiteral("AUX-Batterie Spannung sehr niedrig - evtl. Austausch n\u00F6tig");

    en[QStringLiteral("batteryAuxVoltageVeryLowCharge")] = QStringLiteral("AUX Battery voltage very low - insert main battery to charge");
    de[QStringLiteral("batteryAuxVoltageVeryLowCharge")] = QStringLiteral("AUX-Batterie Spannung sehr niedrig - Hauptbatterie einsetzen zum Laden");

    en[QStringLiteral("batteryEmptyRecharge")] = QStringLiteral("Battery empty. Recharge battery");
    de[QStringLiteral("batteryEmptyRecharge")] = QStringLiteral("Batterie leer. Bitte aufladen");

    en[QStringLiteral("batteryMaxSpeedReduced")] = QStringLiteral("Max speed is reduced. Battery is below 5%");
    de[QStringLiteral("batteryMaxSpeedReduced")] = QStringLiteral("H\u00F6chstgeschwindigkeit reduziert. Batterie unter 5%");

    en[QStringLiteral("batteryLowPowerReduced")] = QStringLiteral("Battery low. Power reduced. Please recharge battery");
    de[QStringLiteral("batteryLowPowerReduced")] = QStringLiteral("Batterie schwach. Leistung reduziert. Bitte aufladen");

    en[QStringLiteral("batterySlot0")] = QStringLiteral("Battery 0");
    de[QStringLiteral("batterySlot0")] = QStringLiteral("Batterie 0");

    en[QStringLiteral("batterySlot1")] = QStringLiteral("Battery 1");
    de[QStringLiteral("batterySlot1")] = QStringLiteral("Batterie 1");

    // -----------------------------------------------------------------------
    // Speed & power
    // -----------------------------------------------------------------------

    en[QStringLiteral("warningHandlebarLocked")] = QStringLiteral("Handlebar locked \u2014 turn all the way left to unlock");
    de[QStringLiteral("warningHandlebarLocked")] = QStringLiteral("Lenker verriegelt \u2014 ganz nach links drehen zum Entriegeln");

    en[QStringLiteral("warningLowTemperature")] = QStringLiteral("Low temperature detected. Reduced performance possible.");
    de[QStringLiteral("warningLowTemperature")] = QStringLiteral("Niedrige Temperatur erkannt. Eingeschr\u00E4nkte Leistung m\u00F6glich.");

    en[QStringLiteral("speedKmh")] = QStringLiteral("km/h");
    de[QStringLiteral("speedKmh")] = QStringLiteral("km/h");

    en[QStringLiteral("powerRegen")] = QStringLiteral("REGEN");
    de[QStringLiteral("powerRegen")] = QStringLiteral("REKU");

    en[QStringLiteral("powerDischarge")] = QStringLiteral("DISCHARGE");
    de[QStringLiteral("powerDischarge")] = QStringLiteral("ENTLADUNG");

    // -----------------------------------------------------------------------
    // Fault codes
    // -----------------------------------------------------------------------

    en[QStringLiteral("faultSignalWireBroken")] = QStringLiteral("Signal wire broken");
    de[QStringLiteral("faultSignalWireBroken")] = QStringLiteral("Signaldraht defekt");

    en[QStringLiteral("faultCriticalOverTemp")] = QStringLiteral("Critical over-temperature");
    de[QStringLiteral("faultCriticalOverTemp")] = QStringLiteral("Kritische \u00DCbertemperatur");

    en[QStringLiteral("faultShortCircuit")] = QStringLiteral("Short circuit");
    de[QStringLiteral("faultShortCircuit")] = QStringLiteral("Kurzschluss");

    en[QStringLiteral("faultBmsNotFollowing")] = QStringLiteral("BMS not following commands");
    de[QStringLiteral("faultBmsNotFollowing")] = QStringLiteral("BMS reagiert nicht auf Befehle");

    en[QStringLiteral("faultBmsCommError")] = QStringLiteral("BMS communication error");
    de[QStringLiteral("faultBmsCommError")] = QStringLiteral("BMS-Kommunikationsfehler");

    en[QStringLiteral("faultNfcReaderError")] = QStringLiteral("NFC reader error");
    de[QStringLiteral("faultNfcReaderError")] = QStringLiteral("NFC-Leser Fehler");

    en[QStringLiteral("faultOverTempCharging")] = QStringLiteral("Over-temperature while charging");
    de[QStringLiteral("faultOverTempCharging")] = QStringLiteral("\u00DCbertemperatur beim Laden");

    en[QStringLiteral("faultUnderTempCharging")] = QStringLiteral("Under-temperature while charging");
    de[QStringLiteral("faultUnderTempCharging")] = QStringLiteral("Untertemperatur beim Laden");

    en[QStringLiteral("faultOverTempDischarging")] = QStringLiteral("Over-temperature while discharging");
    de[QStringLiteral("faultOverTempDischarging")] = QStringLiteral("\u00DCbertemperatur beim Entladen");

    en[QStringLiteral("faultUnderTempDischarging")] = QStringLiteral("Under-temperature while discharging");
    de[QStringLiteral("faultUnderTempDischarging")] = QStringLiteral("Untertemperatur beim Entladen");

    en[QStringLiteral("faultMosfetOverTemp")] = QStringLiteral("MOSFET over-temperature");
    de[QStringLiteral("faultMosfetOverTemp")] = QStringLiteral("MOSFET-\u00DCbertemperatur");

    en[QStringLiteral("faultCellOverVoltage")] = QStringLiteral("Cell over-voltage");
    de[QStringLiteral("faultCellOverVoltage")] = QStringLiteral("Zellen-\u00DCberspannung");

    en[QStringLiteral("faultCellUnderVoltage")] = QStringLiteral("Cell under-voltage");
    de[QStringLiteral("faultCellUnderVoltage")] = QStringLiteral("Zellen-Unterspannung");

    en[QStringLiteral("faultOverCurrentCharging")] = QStringLiteral("Over-current while charging");
    de[QStringLiteral("faultOverCurrentCharging")] = QStringLiteral("\u00DCberstrom beim Laden");

    en[QStringLiteral("faultOverCurrentDischarging")] = QStringLiteral("Over-current while discharging");
    de[QStringLiteral("faultOverCurrentDischarging")] = QStringLiteral("\u00DCberstrom beim Entladen");

    en[QStringLiteral("faultPackOverVoltage")] = QStringLiteral("Pack over-voltage");
    de[QStringLiteral("faultPackOverVoltage")] = QStringLiteral("Pack-\u00DCberspannung");

    en[QStringLiteral("faultPackUnderVoltage")] = QStringLiteral("Pack under-voltage");
    de[QStringLiteral("faultPackUnderVoltage")] = QStringLiteral("Pack-Unterspannung");

    en[QStringLiteral("faultReserved")] = QStringLiteral("Reserved");
    de[QStringLiteral("faultReserved")] = QStringLiteral("Reserviert");

    en[QStringLiteral("faultBmsZeroData")] = QStringLiteral("BMS has zero data");
    de[QStringLiteral("faultBmsZeroData")] = QStringLiteral("BMS hat keine Daten");

    en[QStringLiteral("faultUnknown")] = QStringLiteral("Unknown fault");
    de[QStringLiteral("faultUnknown")] = QStringLiteral("Unbekannter Fehler");

    en[QStringLiteral("faultMultipleCritical")] = QStringLiteral("Multiple Critical Issues");
    de[QStringLiteral("faultMultipleCritical")] = QStringLiteral("Mehrere kritische Probleme");

    en[QStringLiteral("faultMultipleBattery")] = QStringLiteral("Multiple Battery Issues");
    de[QStringLiteral("faultMultipleBattery")] = QStringLiteral("Mehrere Batterieprobleme");

    // ECU fault codes (Exx)

    en[QStringLiteral("faultEcuBatteryOverVoltage")] = QStringLiteral("Battery over-voltage");
    de[QStringLiteral("faultEcuBatteryOverVoltage")] = QStringLiteral("Batterie-\u00DCberspannung");

    en[QStringLiteral("faultEcuBatteryUnderVoltage")] = QStringLiteral("Battery under-voltage");
    de[QStringLiteral("faultEcuBatteryUnderVoltage")] = QStringLiteral("Batterie-Unterspannung");

    en[QStringLiteral("faultEcuMotorShortCircuit")] = QStringLiteral("Motor short-circuit");
    de[QStringLiteral("faultEcuMotorShortCircuit")] = QStringLiteral("Motor-Kurzschluss");

    en[QStringLiteral("faultEcuMotorStalled")] = QStringLiteral("Motor stalled");
    de[QStringLiteral("faultEcuMotorStalled")] = QStringLiteral("Motor blockiert");

    en[QStringLiteral("faultEcuHallSensor")] = QStringLiteral("Hall sensor fault");
    de[QStringLiteral("faultEcuHallSensor")] = QStringLiteral("Hallsensor-Fehler");

    en[QStringLiteral("faultEcuMosfet")] = QStringLiteral("MOSFET fault");
    de[QStringLiteral("faultEcuMosfet")] = QStringLiteral("MOSFET-Fehler");

    en[QStringLiteral("faultEcuMotorOpenCircuit")] = QStringLiteral("Motor open-circuit");
    de[QStringLiteral("faultEcuMotorOpenCircuit")] = QStringLiteral("Motor-Leitungsbruch");

    en[QStringLiteral("faultEcuSelfCheck")] = QStringLiteral("Self-check error");
    de[QStringLiteral("faultEcuSelfCheck")] = QStringLiteral("Selbsttest-Fehler");

    en[QStringLiteral("faultEcuOverTemperature")] = QStringLiteral("Controller over-temperature");
    de[QStringLiteral("faultEcuOverTemperature")] = QStringLiteral("Steuerger\u00E4t-\u00DCbertemperatur");

    en[QStringLiteral("faultEcuThrottleAbnormal")] = QStringLiteral("Throttle fault");
    de[QStringLiteral("faultEcuThrottleAbnormal")] = QStringLiteral("Gasgriff-Fehler");

    en[QStringLiteral("faultEcuMotorTemperature")] = QStringLiteral("Motor over-temperature");
    de[QStringLiteral("faultEcuMotorTemperature")] = QStringLiteral("Motor-\u00DCbertemperatur");

    en[QStringLiteral("faultEcuThrottleAtPowerUp")] = QStringLiteral("Throttle held at power-up");
    de[QStringLiteral("faultEcuThrottleAtPowerUp")] = QStringLiteral("Gasgriff beim Einschalten aktiv");

    en[QStringLiteral("faultEcuInternal15v")] = QStringLiteral("Internal 15V fault");
    de[QStringLiteral("faultEcuInternal15v")] = QStringLiteral("Interner 15V-Fehler");

    en[QStringLiteral("faultEcuCommLost")] = QStringLiteral("ECU communication lost");
    de[QStringLiteral("faultEcuCommLost")] = QStringLiteral("ECU-Kommunikation unterbrochen");

    // -----------------------------------------------------------------------
    // Status bar & odometer
    // -----------------------------------------------------------------------

    en[QStringLiteral("statusBarDuration")] = QStringLiteral("DURATION");
    de[QStringLiteral("statusBarDuration")] = QStringLiteral("DAUER");

    en[QStringLiteral("statusBarAvgSpeed")] = QStringLiteral("\u00D8 SPEED");
    de[QStringLiteral("statusBarAvgSpeed")] = QStringLiteral("\u00D8 TEMPO");

    en[QStringLiteral("statusBarTrip")] = QStringLiteral("TRIP");
    de[QStringLiteral("statusBarTrip")] = QStringLiteral("STRECKE");

    en[QStringLiteral("statusBarTotal")] = QStringLiteral("TOTAL");
    de[QStringLiteral("statusBarTotal")] = QStringLiteral("GESAMT");

    en[QStringLiteral("statusBarKmh")] = QStringLiteral("km/h");
    de[QStringLiteral("statusBarKmh")] = QStringLiteral("km/h");

    en[QStringLiteral("odometerTrip")] = QStringLiteral("TRIP");
    de[QStringLiteral("odometerTrip")] = QStringLiteral("STRECKE");

    en[QStringLiteral("odometerTotal")] = QStringLiteral("TOTAL");
    de[QStringLiteral("odometerTotal")] = QStringLiteral("GESAMT");

    en[QStringLiteral("odometerAvgSpeed")] = QStringLiteral("AVG SPEED");
    de[QStringLiteral("odometerAvgSpeed")] = QStringLiteral("\u00D8 TEMPO");

    en[QStringLiteral("odometerTripTime")] = QStringLiteral("TRIP TIME");
    de[QStringLiteral("odometerTripTime")] = QStringLiteral("FAHRZEIT");

    // -----------------------------------------------------------------------
    // Address entry
    // -----------------------------------------------------------------------

    en[QStringLiteral("navEnterCity")] = QStringLiteral("ENTER CITY");
    de[QStringLiteral("navEnterCity")] = QStringLiteral("ORT EINGEBEN");

    en[QStringLiteral("navSelectCity")] = QStringLiteral("SELECT CITY");
    de[QStringLiteral("navSelectCity")] = QStringLiteral("ORT AUSW\u00C4HLEN");

    en[QStringLiteral("navEnterStreet")] = QStringLiteral("ENTER STREET");
    de[QStringLiteral("navEnterStreet")] = QStringLiteral("STRASSE EINGEBEN");

    en[QStringLiteral("navSelectStreet")] = QStringLiteral("SELECT STREET");
    de[QStringLiteral("navSelectStreet")] = QStringLiteral("STRASSE AUSW\u00C4HLEN");

    en[QStringLiteral("navSelectNumber")] = QStringLiteral("SELECT NUMBER");
    de[QStringLiteral("navSelectNumber")] = QStringLiteral("HAUSNUMMER W\u00C4HLEN");

    en[QStringLiteral("navConfirmDestination")] = QStringLiteral("CONFIRM DESTINATION");
    de[QStringLiteral("navConfirmDestination")] = QStringLiteral("ZIEL BEST\u00C4TIGEN");

    en[QStringLiteral("navCities")] = QStringLiteral("cities");
    de[QStringLiteral("navCities")] = QStringLiteral("Orte");

    en[QStringLiteral("navStreets")] = QStringLiteral("streets");
    de[QStringLiteral("navStreets")] = QStringLiteral("Stra\u00DFen");

    en[QStringLiteral("navNoMatches")] = QStringLiteral("No matches");
    de[QStringLiteral("navNoMatches")] = QStringLiteral("Keine Treffer");

    en[QStringLiteral("addressLoading")] = QStringLiteral("Loading address database...");
    de[QStringLiteral("addressLoading")] = QStringLiteral("Adressdatenbank wird geladen...");

    en[QStringLiteral("addressMapNotFound")] = QStringLiteral("Map file not found.");
    de[QStringLiteral("addressMapNotFound")] = QStringLiteral("Kartendatei nicht gefunden.");

    // -----------------------------------------------------------------------
    // Standby
    // -----------------------------------------------------------------------

    en[QStringLiteral("standbyWarning")] = QStringLiteral("Vehicle will enter standby in");
    de[QStringLiteral("standbyWarning")] = QStringLiteral("Fahrzeug geht in Standby in");

    en[QStringLiteral("standbySeconds")] = QStringLiteral("seconds");
    de[QStringLiteral("standbySeconds")] = QStringLiteral("Sekunden");

    en[QStringLiteral("standbyCancel")] = QStringLiteral("Press brake or move kickstand to cancel");
    de[QStringLiteral("standbyCancel")] = QStringLiteral("Bremse dr\u00FCcken oder Seitenst\u00E4nder bewegen zum Abbrechen");

    // -----------------------------------------------------------------------
    // Destination & map
    // -----------------------------------------------------------------------

    en[QStringLiteral("destinationOfflineOnly")] = QStringLiteral("The destination selector only works with offline maps");
    de[QStringLiteral("destinationOfflineOnly")] = QStringLiteral("Die Zielauswahl funktioniert nur mit Offline-Karten");

    en[QStringLiteral("destinationInstallMapData")] = QStringLiteral("Please install the map data to use this feature");
    de[QStringLiteral("destinationInstallMapData")] = QStringLiteral("Bitte installiere die Kartendaten, um diese Funktion zu nutzen");

    en[QStringLiteral("mapWaitingForGps")] = QStringLiteral("Waiting for GPS fix");
    de[QStringLiteral("mapWaitingForGps")] = QStringLiteral("Warte auf GPS-Signal");

    en[QStringLiteral("mapOutOfCoverage")] = QStringLiteral("No map data for current location");
    de[QStringLiteral("mapOutOfCoverage")] = QStringLiteral("Keine Kartendaten f\u00FCr den aktuellen Standort");

    // -----------------------------------------------------------------------
    // Shortcut menu
    // -----------------------------------------------------------------------

    en[QStringLiteral("shortcutPressToConfirm")] = QStringLiteral("Press to confirm");
    de[QStringLiteral("shortcutPressToConfirm")] = QStringLiteral("Zum Best\u00E4tigen dr\u00FCcken");
}

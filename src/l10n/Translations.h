#pragma once

#include <QObject>
#include <QHash>

class Translations : public QObject
{
    Q_OBJECT

    // Menu strings
    Q_PROPERTY(QString menuTitle READ menuTitle NOTIFY languageChanged)
    Q_PROPERTY(QString menuSettings READ menuSettings NOTIFY languageChanged)
    Q_PROPERTY(QString menuTheme READ menuTheme NOTIFY languageChanged)
    Q_PROPERTY(QString menuThemeAuto READ menuThemeAuto NOTIFY languageChanged)
    Q_PROPERTY(QString menuThemeDark READ menuThemeDark NOTIFY languageChanged)
    Q_PROPERTY(QString menuThemeLight READ menuThemeLight NOTIFY languageChanged)
    Q_PROPERTY(QString menuLanguage READ menuLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString menuStatusBar READ menuStatusBar NOTIFY languageChanged)
    Q_PROPERTY(QString menuBatteryDisplay READ menuBatteryDisplay NOTIFY languageChanged)
    Q_PROPERTY(QString menuBatteryPercentage READ menuBatteryPercentage NOTIFY languageChanged)
    Q_PROPERTY(QString menuBatteryRange READ menuBatteryRange NOTIFY languageChanged)
    Q_PROPERTY(QString menuGpsIcon READ menuGpsIcon NOTIFY languageChanged)
    Q_PROPERTY(QString menuBluetoothIcon READ menuBluetoothIcon NOTIFY languageChanged)
    Q_PROPERTY(QString menuCloudIcon READ menuCloudIcon NOTIFY languageChanged)
    Q_PROPERTY(QString menuInternetIcon READ menuInternetIcon NOTIFY languageChanged)
    Q_PROPERTY(QString menuClock READ menuClock NOTIFY languageChanged)
    Q_PROPERTY(QString menuBlinkerStyle READ menuBlinkerStyle NOTIFY languageChanged)
    Q_PROPERTY(QString menuBlinkerIcon READ menuBlinkerIcon NOTIFY languageChanged)
    Q_PROPERTY(QString menuBlinkerOverlay READ menuBlinkerOverlay NOTIFY languageChanged)
    Q_PROPERTY(QString menuBatteryMode READ menuBatteryMode NOTIFY languageChanged)
    Q_PROPERTY(QString menuBatterySingle READ menuBatterySingle NOTIFY languageChanged)
    Q_PROPERTY(QString menuBatteryDual READ menuBatteryDual NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarm READ menuAlarm NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarmEnable READ menuAlarmEnable NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarmHonk READ menuAlarmHonk NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarmDuration READ menuAlarmDuration NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarmDuration10 READ menuAlarmDuration10 NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarmDuration20 READ menuAlarmDuration20 NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarmDuration30 READ menuAlarmDuration30 NOTIFY languageChanged)
    Q_PROPERTY(QString menuSystem READ menuSystem NOTIFY languageChanged)
    Q_PROPERTY(QString menuEnterUms READ menuEnterUms NOTIFY languageChanged)
    Q_PROPERTY(QString menuResetTrip READ menuResetTrip NOTIFY languageChanged)
    Q_PROPERTY(QString menuAbout READ menuAbout NOTIFY languageChanged)
    Q_PROPERTY(QString menuExit READ menuExit NOTIFY languageChanged)
    Q_PROPERTY(QString menuMapNav READ menuMapNav NOTIFY languageChanged)
    Q_PROPERTY(QString menuRenderMode READ menuRenderMode NOTIFY languageChanged)
    Q_PROPERTY(QString menuVector READ menuVector NOTIFY languageChanged)
    Q_PROPERTY(QString menuRaster READ menuRaster NOTIFY languageChanged)
    Q_PROPERTY(QString menuMapType READ menuMapType NOTIFY languageChanged)
    Q_PROPERTY(QString menuOnline READ menuOnline NOTIFY languageChanged)
    Q_PROPERTY(QString menuOffline READ menuOffline NOTIFY languageChanged)
    Q_PROPERTY(QString menuNavRouting READ menuNavRouting NOTIFY languageChanged)
    Q_PROPERTY(QString menuOnlineOsm READ menuOnlineOsm NOTIFY languageChanged)

    // Visibility options
    Q_PROPERTY(QString optAlways READ optAlways NOTIFY languageChanged)
    Q_PROPERTY(QString optActiveOrError READ optActiveOrError NOTIFY languageChanged)
    Q_PROPERTY(QString optErrorOnly READ optErrorOnly NOTIFY languageChanged)
    Q_PROPERTY(QString optNever READ optNever NOTIFY languageChanged)

    // Control hints
    Q_PROPERTY(QString controlBack READ controlBack NOTIFY languageChanged)
    Q_PROPERTY(QString controlExit READ controlExit NOTIFY languageChanged)
    Q_PROPERTY(QString controlSelect READ controlSelect NOTIFY languageChanged)
    Q_PROPERTY(QString controlNext READ controlNext NOTIFY languageChanged)
    Q_PROPERTY(QString controlScroll READ controlScroll NOTIFY languageChanged)

    // M6: Shutdown / overlays
    Q_PROPERTY(QString shuttingDown READ shuttingDown NOTIFY languageChanged)
    Q_PROPERTY(QString connectingTitle READ connectingTitle NOTIFY languageChanged)
    Q_PROPERTY(QString connectingExplanation READ connectingExplanation NOTIFY languageChanged)
    Q_PROPERTY(QString connectingBypassHint READ connectingBypassHint NOTIFY languageChanged)
    Q_PROPERTY(QString umsPreparing READ umsPreparing NOTIFY languageChanged)
    Q_PROPERTY(QString umsActive READ umsActive NOTIFY languageChanged)
    Q_PROPERTY(QString umsConnect READ umsConnect NOTIFY languageChanged)
    Q_PROPERTY(QString umsProcessing READ umsProcessing NOTIFY languageChanged)
    Q_PROPERTY(QString blePinPrompt READ blePinPrompt NOTIFY languageChanged)
    Q_PROPERTY(QString hibernatePrompt READ hibernatePrompt NOTIFY languageChanged)
    Q_PROPERTY(QString hibernateTapKeycard READ hibernateTapKeycard NOTIFY languageChanged)
    Q_PROPERTY(QString hibernateSeatboxOpen READ hibernateSeatboxOpen NOTIFY languageChanged)
    Q_PROPERTY(QString hibernateCloseSeatbox READ hibernateCloseSeatbox NOTIFY languageChanged)
    Q_PROPERTY(QString hibernating READ hibernating NOTIFY languageChanged)
    Q_PROPERTY(QString aboutTitle READ aboutTitle NOTIFY languageChanged)
    Q_PROPERTY(QString nonCommercialLicense READ nonCommercialLicense NOTIFY languageChanged)

    // M7: Navigation
    Q_PROPERTY(QString navCalculating READ navCalculating NOTIFY languageChanged)
    Q_PROPERTY(QString navRecalculating READ navRecalculating NOTIFY languageChanged)
    Q_PROPERTY(QString navArrived READ navArrived NOTIFY languageChanged)
    Q_PROPERTY(QString navOffRoute READ navOffRoute NOTIFY languageChanged)
    Q_PROPERTY(QString navSetDestination READ navSetDestination NOTIFY languageChanged)
    Q_PROPERTY(QString navUnavailable READ navUnavailable NOTIFY languageChanged)
    Q_PROPERTY(QString navEnterCode READ navEnterCode NOTIFY languageChanged)
    Q_PROPERTY(QString navConfirmDest READ navConfirmDest NOTIFY languageChanged)
    Q_PROPERTY(QString navThen READ navThen NOTIFY languageChanged)

    Q_PROPERTY(QString language READ language NOTIFY languageChanged)

public:
    explicit Translations(QObject *parent = nullptr);

    Q_INVOKABLE void setLanguage(const QString &lang);
    QString language() const { return m_language; }

    // Getters - use lookup() not Qt's tr()
    QString menuTitle() const { return lookup("menuTitle"); }
    QString menuSettings() const { return lookup("menuSettings"); }
    QString menuTheme() const { return lookup("menuTheme"); }
    QString menuThemeAuto() const { return lookup("menuThemeAuto"); }
    QString menuThemeDark() const { return lookup("menuThemeDark"); }
    QString menuThemeLight() const { return lookup("menuThemeLight"); }
    QString menuLanguage() const { return lookup("menuLanguage"); }
    QString menuStatusBar() const { return lookup("menuStatusBar"); }
    QString menuBatteryDisplay() const { return lookup("menuBatteryDisplay"); }
    QString menuBatteryPercentage() const { return lookup("menuBatteryPercentage"); }
    QString menuBatteryRange() const { return lookup("menuBatteryRange"); }
    QString menuGpsIcon() const { return lookup("menuGpsIcon"); }
    QString menuBluetoothIcon() const { return lookup("menuBluetoothIcon"); }
    QString menuCloudIcon() const { return lookup("menuCloudIcon"); }
    QString menuInternetIcon() const { return lookup("menuInternetIcon"); }
    QString menuClock() const { return lookup("menuClock"); }
    QString menuBlinkerStyle() const { return lookup("menuBlinkerStyle"); }
    QString menuBlinkerIcon() const { return lookup("menuBlinkerIcon"); }
    QString menuBlinkerOverlay() const { return lookup("menuBlinkerOverlay"); }
    QString menuBatteryMode() const { return lookup("menuBatteryMode"); }
    QString menuBatterySingle() const { return lookup("menuBatterySingle"); }
    QString menuBatteryDual() const { return lookup("menuBatteryDual"); }
    QString menuAlarm() const { return lookup("menuAlarm"); }
    QString menuAlarmEnable() const { return lookup("menuAlarmEnable"); }
    QString menuAlarmHonk() const { return lookup("menuAlarmHonk"); }
    QString menuAlarmDuration() const { return lookup("menuAlarmDuration"); }
    QString menuAlarmDuration10() const { return lookup("menuAlarmDuration10"); }
    QString menuAlarmDuration20() const { return lookup("menuAlarmDuration20"); }
    QString menuAlarmDuration30() const { return lookup("menuAlarmDuration30"); }
    QString menuSystem() const { return lookup("menuSystem"); }
    QString menuEnterUms() const { return lookup("menuEnterUms"); }
    QString menuResetTrip() const { return lookup("menuResetTrip"); }
    QString menuAbout() const { return lookup("menuAbout"); }
    QString menuExit() const { return lookup("menuExit"); }
    QString menuMapNav() const { return lookup("menuMapNav"); }
    QString menuRenderMode() const { return lookup("menuRenderMode"); }
    QString menuVector() const { return lookup("menuVector"); }
    QString menuRaster() const { return lookup("menuRaster"); }
    QString menuMapType() const { return lookup("menuMapType"); }
    QString menuOnline() const { return lookup("menuOnline"); }
    QString menuOffline() const { return lookup("menuOffline"); }
    QString menuNavRouting() const { return lookup("menuNavRouting"); }
    QString menuOnlineOsm() const { return lookup("menuOnlineOsm"); }

    QString optAlways() const { return lookup("optAlways"); }
    QString optActiveOrError() const { return lookup("optActiveOrError"); }
    QString optErrorOnly() const { return lookup("optErrorOnly"); }
    QString optNever() const { return lookup("optNever"); }

    QString controlBack() const { return lookup("controlBack"); }
    QString controlExit() const { return lookup("controlExit"); }
    QString controlSelect() const { return lookup("controlSelect"); }
    QString controlNext() const { return lookup("controlNext"); }
    QString controlScroll() const { return lookup("controlScroll"); }

    QString shuttingDown() const { return lookup("shuttingDown"); }
    QString connectingTitle() const { return lookup("connectingTitle"); }
    QString connectingExplanation() const { return lookup("connectingExplanation"); }
    QString connectingBypassHint() const { return lookup("connectingBypassHint"); }
    QString umsPreparing() const { return lookup("umsPreparing"); }
    QString umsActive() const { return lookup("umsActive"); }
    QString umsConnect() const { return lookup("umsConnect"); }
    QString umsProcessing() const { return lookup("umsProcessing"); }
    QString blePinPrompt() const { return lookup("blePinPrompt"); }
    QString hibernatePrompt() const { return lookup("hibernatePrompt"); }
    QString hibernateTapKeycard() const { return lookup("hibernateTapKeycard"); }
    QString hibernateSeatboxOpen() const { return lookup("hibernateSeatboxOpen"); }
    QString hibernateCloseSeatbox() const { return lookup("hibernateCloseSeatbox"); }
    QString hibernating() const { return lookup("hibernating"); }
    QString aboutTitle() const { return lookup("aboutTitle"); }
    QString nonCommercialLicense() const { return lookup("nonCommercialLicense"); }

    QString navCalculating() const { return lookup("navCalculating"); }
    QString navRecalculating() const { return lookup("navRecalculating"); }
    QString navArrived() const { return lookup("navArrived"); }
    QString navOffRoute() const { return lookup("navOffRoute"); }
    QString navSetDestination() const { return lookup("navSetDestination"); }
    QString navUnavailable() const { return lookup("navUnavailable"); }
    QString navEnterCode() const { return lookup("navEnterCode"); }
    QString navConfirmDest() const { return lookup("navConfirmDest"); }
    QString navThen() const { return lookup("navThen"); }

signals:
    void languageChanged();

private:
    QString lookup(const char *key) const;
    void initStrings();

    QString m_language = QStringLiteral("en");
    QHash<QString, QHash<QString, QString>> m_strings;
};

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
    Q_PROPERTY(QString menuFaults READ menuFaults NOTIFY languageChanged)
    Q_PROPERTY(QString faultsEmpty READ faultsEmpty NOTIFY languageChanged)
    Q_PROPERTY(QString faultActive READ faultActive NOTIFY languageChanged)
    Q_PROPERTY(QString faultCleared READ faultCleared NOTIFY languageChanged)
    Q_PROPERTY(QString updateModeTitle READ updateModeTitle NOTIFY languageChanged)
    Q_PROPERTY(QString updateModeBody1 READ updateModeBody1 NOTIFY languageChanged)
    Q_PROPERTY(QString updateModeBody2 READ updateModeBody2 NOTIFY languageChanged)
    Q_PROPERTY(QString updateModeBody3 READ updateModeBody3 NOTIFY languageChanged)
    Q_PROPERTY(QString updateModeScanHint READ updateModeScanHint NOTIFY languageChanged)
    Q_PROPERTY(QString updateModeStart READ updateModeStart NOTIFY languageChanged)
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
    Q_PROPERTY(QString menuMapUpdateCheck READ menuMapUpdateCheck NOTIFY languageChanged)
    Q_PROPERTY(QString menuMapAutoDownload READ menuMapAutoDownload NOTIFY languageChanged)
    Q_PROPERTY(QString menuOnlineOsm READ menuOnlineOsm NOTIFY languageChanged)

    // Menu headers
    Q_PROPERTY(QString menuNavigationHeader READ menuNavigationHeader NOTIFY languageChanged)
    Q_PROPERTY(QString menuSettingsHeader READ menuSettingsHeader NOTIFY languageChanged)
    Q_PROPERTY(QString menuThemeHeader READ menuThemeHeader NOTIFY languageChanged)
    Q_PROPERTY(QString menuLanguageHeader READ menuLanguageHeader NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarmHeader READ menuAlarmHeader NOTIFY languageChanged)
    Q_PROPERTY(QString menuSavedLocationsHeader READ menuSavedLocationsHeader NOTIFY languageChanged)
    Q_PROPERTY(QString menuAlarmDurationHeader READ menuAlarmDurationHeader NOTIFY languageChanged)

    // Menu actions
    Q_PROPERTY(QString menuToggleHazardLights READ menuToggleHazardLights NOTIFY languageChanged)
    Q_PROPERTY(QString menuHopOn READ menuHopOn NOTIFY languageChanged)
    Q_PROPERTY(QString menuHopOnHeader READ menuHopOnHeader NOTIFY languageChanged)
    Q_PROPERTY(QString menuHopOnActivate READ menuHopOnActivate NOTIFY languageChanged)
    Q_PROPERTY(QString menuHopOnActivateTop READ menuHopOnActivateTop NOTIFY languageChanged)
    Q_PROPERTY(QString menuHopOnRelearn READ menuHopOnRelearn NOTIFY languageChanged)
    Q_PROPERTY(QString menuHopOnDisable READ menuHopOnDisable NOTIFY languageChanged)
    Q_PROPERTY(QString hopOnLearnTitle READ hopOnLearnTitle NOTIFY languageChanged)
    Q_PROPERTY(QString hopOnLearnHint READ hopOnLearnHint NOTIFY languageChanged)
    Q_PROPERTY(QString hopOnLockedTitle READ hopOnLockedTitle NOTIFY languageChanged)
    Q_PROPERTY(QString hopOnLockedHint READ hopOnLockedHint NOTIFY languageChanged)
    Q_PROPERTY(QString hopOnSavedToast READ hopOnSavedToast NOTIFY languageChanged)
    Q_PROPERTY(QString hopOnAbortedToast READ hopOnAbortedToast NOTIFY languageChanged)
    Q_PROPERTY(QString menuSwitchToCluster READ menuSwitchToCluster NOTIFY languageChanged)
    Q_PROPERTY(QString menuSwitchToMap READ menuSwitchToMap NOTIFY languageChanged)
    Q_PROPERTY(QString menuEnterDestinationCode READ menuEnterDestinationCode NOTIFY languageChanged)
    Q_PROPERTY(QString menuDeleteLocation READ menuDeleteLocation NOTIFY languageChanged)
    Q_PROPERTY(QString menuStartNavigation READ menuStartNavigation NOTIFY languageChanged)
    Q_PROPERTY(QString menuStopNavigation READ menuStopNavigation NOTIFY languageChanged)

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
    Q_PROPERTY(QString controlCancel READ controlCancel NOTIFY languageChanged)
    Q_PROPERTY(QString navGo READ navGo NOTIFY languageChanged)
    Q_PROPERTY(QString controlLeftBrake READ controlLeftBrake NOTIFY languageChanged)
    Q_PROPERTY(QString controlRightBrake READ controlRightBrake NOTIFY languageChanged)
    Q_PROPERTY(QString controlNextItem READ controlNextItem NOTIFY languageChanged)
    Q_PROPERTY(QString controlPressRightBrakeConfirm READ controlPressRightBrakeConfirm NOTIFY languageChanged)
    Q_PROPERTY(QString controlPressLeftBrakeEdit READ controlPressLeftBrakeEdit NOTIFY languageChanged)

    // Shutdown / overlays
    Q_PROPERTY(QString shuttingDown READ shuttingDown NOTIFY languageChanged)
    Q_PROPERTY(QString shutdownComplete READ shutdownComplete NOTIFY languageChanged)
    Q_PROPERTY(QString shutdownSuspending READ shutdownSuspending NOTIFY languageChanged)
    Q_PROPERTY(QString shutdownHibernationImminent READ shutdownHibernationImminent NOTIFY languageChanged)
    Q_PROPERTY(QString shutdownSuspensionImminent READ shutdownSuspensionImminent NOTIFY languageChanged)
    Q_PROPERTY(QString shutdownProcessing READ shutdownProcessing NOTIFY languageChanged)
    Q_PROPERTY(QString connectingTitle READ connectingTitle NOTIFY languageChanged)
    Q_PROPERTY(QString connectingExplanation READ connectingExplanation NOTIFY languageChanged)
    Q_PROPERTY(QString connectingBypassHint READ connectingBypassHint NOTIFY languageChanged)
    Q_PROPERTY(QString connectionLost READ connectionLost NOTIFY languageChanged)
    Q_PROPERTY(QString connectionReconnecting READ connectionReconnecting NOTIFY languageChanged)
    Q_PROPERTY(QString connectionRestored READ connectionRestored NOTIFY languageChanged)

    // UMS
    Q_PROPERTY(QString umsPreparing READ umsPreparing NOTIFY languageChanged)
    Q_PROPERTY(QString umsActive READ umsActive NOTIFY languageChanged)
    Q_PROPERTY(QString umsConnect READ umsConnect NOTIFY languageChanged)
    Q_PROPERTY(QString umsProcessing READ umsProcessing NOTIFY languageChanged)
    Q_PROPERTY(QString umsTitle READ umsTitle NOTIFY languageChanged)
    Q_PROPERTY(QString umsConnectToComputer READ umsConnectToComputer NOTIFY languageChanged)
    Q_PROPERTY(QString umsHoldExit READ umsHoldExit NOTIFY languageChanged)
    Q_PROPERTY(QString controlLeftBrakeHold READ controlLeftBrakeHold NOTIFY languageChanged)

    // Bluetooth
    Q_PROPERTY(QString blePinPrompt READ blePinPrompt NOTIFY languageChanged)
    Q_PROPERTY(QString bluetoothCommError READ bluetoothCommError NOTIFY languageChanged)
    Q_PROPERTY(QString bluetoothPinInstruction READ bluetoothPinInstruction NOTIFY languageChanged)

    // Hibernation
    Q_PROPERTY(QString hibernatePrompt READ hibernatePrompt NOTIFY languageChanged)
    Q_PROPERTY(QString hibernateTapKeycard READ hibernateTapKeycard NOTIFY languageChanged)
    Q_PROPERTY(QString hibernateSeatboxOpen READ hibernateSeatboxOpen NOTIFY languageChanged)
    Q_PROPERTY(QString hibernateCloseSeatbox READ hibernateCloseSeatbox NOTIFY languageChanged)
    Q_PROPERTY(QString hibernating READ hibernating NOTIFY languageChanged)
    Q_PROPERTY(QString hibernationTitle READ hibernationTitle NOTIFY languageChanged)
    Q_PROPERTY(QString hibernationTapKeycardToConfirm READ hibernationTapKeycardToConfirm NOTIFY languageChanged)
    Q_PROPERTY(QString hibernationKeepHoldingBrakes READ hibernationKeepHoldingBrakes NOTIFY languageChanged)
    Q_PROPERTY(QString hibernationOrHoldBrakes READ hibernationOrHoldBrakes NOTIFY languageChanged)
    Q_PROPERTY(QString hibernationCancel READ hibernationCancel NOTIFY languageChanged)
    Q_PROPERTY(QString hibernationKickstand READ hibernationKickstand NOTIFY languageChanged)
    Q_PROPERTY(QString hibernationConfirm READ hibernationConfirm NOTIFY languageChanged)

    // Auto-lock countdown
    Q_PROPERTY(QString autoLockTitle READ autoLockTitle NOTIFY languageChanged)
    Q_PROPERTY(QString autoLockCancelHint READ autoLockCancelHint NOTIFY languageChanged)

    // About
    Q_PROPERTY(QString aboutTitle READ aboutTitle NOTIFY languageChanged)
    Q_PROPERTY(QString aboutNonCommercialTitle READ aboutNonCommercialTitle NOTIFY languageChanged)
    Q_PROPERTY(QString aboutFossDescription READ aboutFossDescription NOTIFY languageChanged)
    Q_PROPERTY(QString aboutOpenSourceComponents READ aboutOpenSourceComponents NOTIFY languageChanged)
    Q_PROPERTY(QString aboutScrollAction READ aboutScrollAction NOTIFY languageChanged)
    Q_PROPERTY(QString aboutBackAction READ aboutBackAction NOTIFY languageChanged)
    Q_PROPERTY(QString aboutBootThemeRestored READ aboutBootThemeRestored NOTIFY languageChanged)
    Q_PROPERTY(QString aboutGenuineAdvantage READ aboutGenuineAdvantage NOTIFY languageChanged)
    Q_PROPERTY(QString nonCommercialLicense READ nonCommercialLicense NOTIFY languageChanged)
    Q_PROPERTY(QString aboutCommercialProhibited READ aboutCommercialProhibited NOTIFY languageChanged)
    Q_PROPERTY(QString aboutScamWarning READ aboutScamWarning NOTIFY languageChanged)
    Q_PROPERTY(QString aboutAuthorizedPartners READ aboutAuthorizedPartners NOTIFY languageChanged)
    Q_PROPERTY(QString aboutSpecialThanks READ aboutSpecialThanks NOTIFY languageChanged)
    Q_PROPERTY(QString aboutPatienceNote READ aboutPatienceNote NOTIFY languageChanged)

    // Toast / monitoring
    Q_PROPERTY(QString lowTempWarning READ lowTempWarning NOTIFY languageChanged)
    Q_PROPERTY(QString lowTempMotor READ lowTempMotor NOTIFY languageChanged)
    Q_PROPERTY(QString lowTempBattery READ lowTempBattery NOTIFY languageChanged)
    Q_PROPERTY(QString lowTemp12vBattery READ lowTemp12vBattery NOTIFY languageChanged)
    Q_PROPERTY(QString bleCommError READ bleCommError NOTIFY languageChanged)
    Q_PROPERTY(QString redisDisconnected READ redisDisconnected NOTIFY languageChanged)
    Q_PROPERTY(QString usbDisconnected READ usbDisconnected NOTIFY languageChanged)
    Q_PROPERTY(QString locationSaved READ locationSaved NOTIFY languageChanged)
    Q_PROPERTY(QString locationDeleted READ locationDeleted NOTIFY languageChanged)
    Q_PROPERTY(QString maxLocationsReached READ maxLocationsReached NOTIFY languageChanged)
    Q_PROPERTY(QString savedLocationsFailed READ savedLocationsFailed NOTIFY languageChanged)
    Q_PROPERTY(QString menuSavedLocations READ menuSavedLocations NOTIFY languageChanged)
    Q_PROPERTY(QString menuSaveLocation READ menuSaveLocation NOTIFY languageChanged)
    Q_PROPERTY(QString menuNavSetup READ menuNavSetup NOTIFY languageChanged)
    Q_PROPERTY(QString plymouthToggled READ plymouthToggled NOTIFY languageChanged)

    // Navigation
    Q_PROPERTY(QString navCalculating READ navCalculating NOTIFY languageChanged)
    Q_PROPERTY(QString navRecalculating READ navRecalculating NOTIFY languageChanged)
    Q_PROPERTY(QString navArrived READ navArrived NOTIFY languageChanged)
    Q_PROPERTY(QString navOffRoute READ navOffRoute NOTIFY languageChanged)
    Q_PROPERTY(QString navSetDestination READ navSetDestination NOTIFY languageChanged)
    Q_PROPERTY(QString navUnavailable READ navUnavailable NOTIFY languageChanged)
    Q_PROPERTY(QString navEnterCode READ navEnterCode NOTIFY languageChanged)
    Q_PROPERTY(QString navConfirmDest READ navConfirmDest NOTIFY languageChanged)
    Q_PROPERTY(QString navThen READ navThen NOTIFY languageChanged)
    Q_PROPERTY(QString navYouHaveArrived READ navYouHaveArrived NOTIFY languageChanged)
    Q_PROPERTY(QString navDistance READ navDistance NOTIFY languageChanged)
    Q_PROPERTY(QString navRemaining READ navRemaining NOTIFY languageChanged)
    Q_PROPERTY(QString navEta READ navEta NOTIFY languageChanged)
    Q_PROPERTY(QString navContinue READ navContinue NOTIFY languageChanged)
    Q_PROPERTY(QString navReturnToRoute READ navReturnToRoute NOTIFY languageChanged)
    Q_PROPERTY(QString navCurrentPositionNotAvailable READ navCurrentPositionNotAvailable NOTIFY languageChanged)
    Q_PROPERTY(QString navCouldNotCalculateRoute READ navCouldNotCalculateRoute NOTIFY languageChanged)
    Q_PROPERTY(QString navDestinationUnreachable READ navDestinationUnreachable NOTIFY languageChanged)
    Q_PROPERTY(QString navNewDestination READ navNewDestination NOTIFY languageChanged)
    Q_PROPERTY(QString navWaitingForGps READ navWaitingForGps NOTIFY languageChanged)
    Q_PROPERTY(QString navWaitingForGpsRoute READ navWaitingForGpsRoute NOTIFY languageChanged)
    Q_PROPERTY(QString navResumingNavigation READ navResumingNavigation NOTIFY languageChanged)
    Q_PROPERTY(QString navArrivedAtDestination READ navArrivedAtDestination NOTIFY languageChanged)
    Q_PROPERTY(QString navOffRouteRerouting READ navOffRouteRerouting NOTIFY languageChanged)
    Q_PROPERTY(QString navCouldNotCalculateNewRoute READ navCouldNotCalculateNewRoute NOTIFY languageChanged)
    Q_PROPERTY(QString navRouteError READ navRouteError NOTIFY languageChanged)

    // Navigation short instructions
    Q_PROPERTY(QString navShortContinueStraight READ navShortContinueStraight NOTIFY languageChanged)
    Q_PROPERTY(QString navShortTurnLeft READ navShortTurnLeft NOTIFY languageChanged)
    Q_PROPERTY(QString navShortTurnRight READ navShortTurnRight NOTIFY languageChanged)
    Q_PROPERTY(QString navShortTurnSlightlyLeft READ navShortTurnSlightlyLeft NOTIFY languageChanged)
    Q_PROPERTY(QString navShortTurnSlightlyRight READ navShortTurnSlightlyRight NOTIFY languageChanged)
    Q_PROPERTY(QString navShortTurnSharplyLeft READ navShortTurnSharplyLeft NOTIFY languageChanged)
    Q_PROPERTY(QString navShortTurnSharplyRight READ navShortTurnSharplyRight NOTIFY languageChanged)
    Q_PROPERTY(QString navShortUturn READ navShortUturn NOTIFY languageChanged)
    Q_PROPERTY(QString navShortUturnRight READ navShortUturnRight NOTIFY languageChanged)
    Q_PROPERTY(QString navShortMerge READ navShortMerge NOTIFY languageChanged)
    Q_PROPERTY(QString navShortMergeLeft READ navShortMergeLeft NOTIFY languageChanged)
    Q_PROPERTY(QString navShortMergeRight READ navShortMergeRight NOTIFY languageChanged)
    Q_PROPERTY(QString navShortContinue READ navShortContinue NOTIFY languageChanged)

    // Navigation setup screen
    Q_PROPERTY(QString navSetupTitle READ navSetupTitle NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupTitleRoutingUnavailable READ navSetupTitleRoutingUnavailable NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupTitleMapsUnavailable READ navSetupTitleMapsUnavailable NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupTitleBothUnavailable READ navSetupTitleBothUnavailable NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupLocalDisplayMaps READ navSetupLocalDisplayMaps NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupRoutingEngine READ navSetupRoutingEngine NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupNoRoutingBody READ navSetupNoRoutingBody NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupScanForInstructions READ navSetupScanForInstructions NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupAllSet READ navSetupAllSet NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDisplayMapsBody READ navSetupDisplayMapsBody NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupRoutingBody READ navSetupRoutingBody NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupCheckingUpdates READ navSetupCheckingUpdates NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadLocating READ navSetupDownloadLocating NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadInstalling READ navSetupDownloadInstalling NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadDone READ navSetupDownloadDone NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadButton READ navSetupDownloadButton NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupUpdateButton READ navSetupUpdateButton NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupResumeButton READ navSetupResumeButton NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadError READ navSetupDownloadError NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadUnsupported READ navSetupDownloadUnsupported NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupInsufficientSpace READ navSetupInsufficientSpace NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadNoInternet READ navSetupDownloadNoInternet NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadWaitingGps READ navSetupDownloadWaitingGps NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadProgress READ navSetupDownloadProgress NOTIFY languageChanged)
    Q_PROPERTY(QString navSetupDownloadProgressBytes READ navSetupDownloadProgressBytes NOTIFY languageChanged)
    Q_PROPERTY(QString mapUpdateAvailableToast READ mapUpdateAvailableToast NOTIFY languageChanged)
    Q_PROPERTY(QString mapUpdateBadge READ mapUpdateBadge NOTIFY languageChanged)
    Q_PROPERTY(QString menuSetupMapMode READ menuSetupMapMode NOTIFY languageChanged)
    Q_PROPERTY(QString menuSetupNavigation READ menuSetupNavigation NOTIFY languageChanged)

    // OTA
    Q_PROPERTY(QString otaDownloading READ otaDownloading NOTIFY languageChanged)
    Q_PROPERTY(QString otaInstalling READ otaInstalling NOTIFY languageChanged)
    Q_PROPERTY(QString otaInitializing READ otaInitializing NOTIFY languageChanged)
    Q_PROPERTY(QString otaCheckingUpdates READ otaCheckingUpdates NOTIFY languageChanged)
    Q_PROPERTY(QString otaCheckFailed READ otaCheckFailed NOTIFY languageChanged)
    Q_PROPERTY(QString otaDeviceUpdated READ otaDeviceUpdated NOTIFY languageChanged)
    Q_PROPERTY(QString otaWaitingDashboard READ otaWaitingDashboard NOTIFY languageChanged)
    Q_PROPERTY(QString otaDownloadingUpdates READ otaDownloadingUpdates NOTIFY languageChanged)
    Q_PROPERTY(QString otaDownloadFailed READ otaDownloadFailed NOTIFY languageChanged)
    Q_PROPERTY(QString otaInstallingUpdates READ otaInstallingUpdates NOTIFY languageChanged)
    Q_PROPERTY(QString otaInstallFailed READ otaInstallFailed NOTIFY languageChanged)
    Q_PROPERTY(QString otaWaitingForReboot READ otaWaitingForReboot NOTIFY languageChanged)
    Q_PROPERTY(QString otaStatusWaitingForReboot READ otaStatusWaitingForReboot NOTIFY languageChanged)
    Q_PROPERTY(QString otaStatusDownloading READ otaStatusDownloading NOTIFY languageChanged)
    Q_PROPERTY(QString otaStatusInstalling READ otaStatusInstalling NOTIFY languageChanged)
    Q_PROPERTY(QString otaUpdate READ otaUpdate NOTIFY languageChanged)
    Q_PROPERTY(QString otaInvalidRelease READ otaInvalidRelease NOTIFY languageChanged)
    Q_PROPERTY(QString otaDownloadFailedShort READ otaDownloadFailedShort NOTIFY languageChanged)
    Q_PROPERTY(QString otaInstallFailedShort READ otaInstallFailedShort NOTIFY languageChanged)
    Q_PROPERTY(QString otaRebootFailed READ otaRebootFailed NOTIFY languageChanged)
    Q_PROPERTY(QString otaUpdateError READ otaUpdateError NOTIFY languageChanged)
    Q_PROPERTY(QString otaPreparingUpdate READ otaPreparingUpdate NOTIFY languageChanged)
    Q_PROPERTY(QString otaPendingReboot READ otaPendingReboot NOTIFY languageChanged)
    Q_PROPERTY(QString otaScooterWillTurnOff READ otaScooterWillTurnOff NOTIFY languageChanged)

    // Battery messages
    Q_PROPERTY(QString batteryKm READ batteryKm NOTIFY languageChanged)
    Q_PROPERTY(QString batteryCbNotCharging READ batteryCbNotCharging NOTIFY languageChanged)
    Q_PROPERTY(QString batteryAuxLowNotCharging READ batteryAuxLowNotCharging NOTIFY languageChanged)
    Q_PROPERTY(QString batteryAuxVoltageLow READ batteryAuxVoltageLow NOTIFY languageChanged)
    Q_PROPERTY(QString batteryAuxVoltageVeryLowReplace READ batteryAuxVoltageVeryLowReplace NOTIFY languageChanged)
    Q_PROPERTY(QString batteryAuxVoltageVeryLowCharge READ batteryAuxVoltageVeryLowCharge NOTIFY languageChanged)
    Q_PROPERTY(QString batteryEmptyRecharge READ batteryEmptyRecharge NOTIFY languageChanged)
    Q_PROPERTY(QString batteryMaxSpeedReduced READ batteryMaxSpeedReduced NOTIFY languageChanged)
    Q_PROPERTY(QString batteryLowPowerReduced READ batteryLowPowerReduced NOTIFY languageChanged)
    Q_PROPERTY(QString batterySlot0 READ batterySlot0 NOTIFY languageChanged)
    Q_PROPERTY(QString batterySlot1 READ batterySlot1 NOTIFY languageChanged)

    // Warnings
    Q_PROPERTY(QString warningHandlebarLocked READ warningHandlebarLocked NOTIFY languageChanged)
    Q_PROPERTY(QString warningLowTemperature READ warningLowTemperature NOTIFY languageChanged)

    // Speed & power
    Q_PROPERTY(QString speedKmh READ speedKmh NOTIFY languageChanged)
    Q_PROPERTY(QString powerRegen READ powerRegen NOTIFY languageChanged)
    Q_PROPERTY(QString powerDischarge READ powerDischarge NOTIFY languageChanged)

    // Fault codes
    Q_PROPERTY(QString faultSignalWireBroken READ faultSignalWireBroken NOTIFY languageChanged)
    Q_PROPERTY(QString faultCriticalOverTemp READ faultCriticalOverTemp NOTIFY languageChanged)
    Q_PROPERTY(QString faultShortCircuit READ faultShortCircuit NOTIFY languageChanged)
    Q_PROPERTY(QString faultBmsNotFollowing READ faultBmsNotFollowing NOTIFY languageChanged)
    Q_PROPERTY(QString faultBmsCommError READ faultBmsCommError NOTIFY languageChanged)
    Q_PROPERTY(QString faultNfcReaderError READ faultNfcReaderError NOTIFY languageChanged)
    Q_PROPERTY(QString faultOverTempCharging READ faultOverTempCharging NOTIFY languageChanged)
    Q_PROPERTY(QString faultUnderTempCharging READ faultUnderTempCharging NOTIFY languageChanged)
    Q_PROPERTY(QString faultOverTempDischarging READ faultOverTempDischarging NOTIFY languageChanged)
    Q_PROPERTY(QString faultUnderTempDischarging READ faultUnderTempDischarging NOTIFY languageChanged)
    Q_PROPERTY(QString faultMosfetOverTemp READ faultMosfetOverTemp NOTIFY languageChanged)
    Q_PROPERTY(QString faultCellOverVoltage READ faultCellOverVoltage NOTIFY languageChanged)
    Q_PROPERTY(QString faultCellUnderVoltage READ faultCellUnderVoltage NOTIFY languageChanged)
    Q_PROPERTY(QString faultOverCurrentCharging READ faultOverCurrentCharging NOTIFY languageChanged)
    Q_PROPERTY(QString faultOverCurrentDischarging READ faultOverCurrentDischarging NOTIFY languageChanged)
    Q_PROPERTY(QString faultPackOverVoltage READ faultPackOverVoltage NOTIFY languageChanged)
    Q_PROPERTY(QString faultPackUnderVoltage READ faultPackUnderVoltage NOTIFY languageChanged)
    Q_PROPERTY(QString faultReserved READ faultReserved NOTIFY languageChanged)
    Q_PROPERTY(QString faultBmsZeroData READ faultBmsZeroData NOTIFY languageChanged)
    Q_PROPERTY(QString faultUnknown READ faultUnknown NOTIFY languageChanged)
    Q_PROPERTY(QString faultMultipleCritical READ faultMultipleCritical NOTIFY languageChanged)
    Q_PROPERTY(QString faultMultipleBattery READ faultMultipleBattery NOTIFY languageChanged)

    // ECU fault codes (Exx)
    Q_PROPERTY(QString faultEcuBatteryOverVoltage READ faultEcuBatteryOverVoltage NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuBatteryUnderVoltage READ faultEcuBatteryUnderVoltage NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuMotorShortCircuit READ faultEcuMotorShortCircuit NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuMotorStalled READ faultEcuMotorStalled NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuHallSensor READ faultEcuHallSensor NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuMosfet READ faultEcuMosfet NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuMotorOpenCircuit READ faultEcuMotorOpenCircuit NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuSelfCheck READ faultEcuSelfCheck NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuOverTemperature READ faultEcuOverTemperature NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuThrottleAbnormal READ faultEcuThrottleAbnormal NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuMotorTemperature READ faultEcuMotorTemperature NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuThrottleAtPowerUp READ faultEcuThrottleAtPowerUp NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuInternal15v READ faultEcuInternal15v NOTIFY languageChanged)
    Q_PROPERTY(QString faultEcuCommLost READ faultEcuCommLost NOTIFY languageChanged)

    // Status bar & odometer
    Q_PROPERTY(QString statusBarDuration READ statusBarDuration NOTIFY languageChanged)
    Q_PROPERTY(QString statusBarAvgSpeed READ statusBarAvgSpeed NOTIFY languageChanged)
    Q_PROPERTY(QString statusBarTrip READ statusBarTrip NOTIFY languageChanged)
    Q_PROPERTY(QString statusBarTotal READ statusBarTotal NOTIFY languageChanged)
    Q_PROPERTY(QString statusBarKmh READ statusBarKmh NOTIFY languageChanged)
    Q_PROPERTY(QString odometerTrip READ odometerTrip NOTIFY languageChanged)
    Q_PROPERTY(QString odometerTotal READ odometerTotal NOTIFY languageChanged)
    Q_PROPERTY(QString odometerAvgSpeed READ odometerAvgSpeed NOTIFY languageChanged)
    Q_PROPERTY(QString odometerTripTime READ odometerTripTime NOTIFY languageChanged)

    // Address entry
    Q_PROPERTY(QString navEnterCity READ navEnterCity NOTIFY languageChanged)
    Q_PROPERTY(QString navSelectCity READ navSelectCity NOTIFY languageChanged)
    Q_PROPERTY(QString navEnterStreet READ navEnterStreet NOTIFY languageChanged)
    Q_PROPERTY(QString navSelectStreet READ navSelectStreet NOTIFY languageChanged)
    Q_PROPERTY(QString navSelectNumber READ navSelectNumber NOTIFY languageChanged)
    Q_PROPERTY(QString navConfirmDestination READ navConfirmDestination NOTIFY languageChanged)
    Q_PROPERTY(QString navCities READ navCities NOTIFY languageChanged)
    Q_PROPERTY(QString navStreets READ navStreets NOTIFY languageChanged)
    Q_PROPERTY(QString navNoMatches READ navNoMatches NOTIFY languageChanged)
    Q_PROPERTY(QString addressLoading READ addressLoading NOTIFY languageChanged)
    Q_PROPERTY(QString addressMapNotFound READ addressMapNotFound NOTIFY languageChanged)

    // Standby
    Q_PROPERTY(QString standbyWarning READ standbyWarning NOTIFY languageChanged)
    Q_PROPERTY(QString standbySeconds READ standbySeconds NOTIFY languageChanged)
    Q_PROPERTY(QString standbyCancel READ standbyCancel NOTIFY languageChanged)

    // Destination & map
    Q_PROPERTY(QString destinationOfflineOnly READ destinationOfflineOnly NOTIFY languageChanged)
    Q_PROPERTY(QString destinationInstallMapData READ destinationInstallMapData NOTIFY languageChanged)
    Q_PROPERTY(QString mapWaitingForGps READ mapWaitingForGps NOTIFY languageChanged)
    Q_PROPERTY(QString mapOutOfCoverage READ mapOutOfCoverage NOTIFY languageChanged)

    // Shortcut menu
    Q_PROPERTY(QString shortcutPressToConfirm READ shortcutPressToConfirm NOTIFY languageChanged)

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
    QString menuFaults() const { return lookup("menuFaults"); }
    QString faultsEmpty() const { return lookup("faultsEmpty"); }
    QString faultActive() const { return lookup("faultActive"); }
    QString faultCleared() const { return lookup("faultCleared"); }
    QString updateModeTitle() const { return lookup("updateModeTitle"); }
    QString updateModeBody1() const { return lookup("updateModeBody1"); }
    QString updateModeBody2() const { return lookup("updateModeBody2"); }
    QString updateModeBody3() const { return lookup("updateModeBody3"); }
    QString updateModeScanHint() const { return lookup("updateModeScanHint"); }
    QString updateModeStart() const { return lookup("updateModeStart"); }
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
    QString menuMapUpdateCheck() const { return lookup("menuMapUpdateCheck"); }
    QString menuMapAutoDownload() const { return lookup("menuMapAutoDownload"); }
    QString menuOnlineOsm() const { return lookup("menuOnlineOsm"); }

    // Menu headers
    QString menuNavigationHeader() const { return lookup("menuNavigationHeader"); }
    QString menuSettingsHeader() const { return lookup("menuSettingsHeader"); }
    QString menuThemeHeader() const { return lookup("menuThemeHeader"); }
    QString menuLanguageHeader() const { return lookup("menuLanguageHeader"); }
    QString menuAlarmHeader() const { return lookup("menuAlarmHeader"); }
    QString menuSavedLocationsHeader() const { return lookup("menuSavedLocationsHeader"); }
    QString menuAlarmDurationHeader() const { return lookup("menuAlarmDurationHeader"); }

    // Menu actions
    QString menuToggleHazardLights() const { return lookup("menuToggleHazardLights"); }
    QString menuHopOn() const { return lookup("menuHopOn"); }
    QString menuHopOnHeader() const { return lookup("menuHopOnHeader"); }
    QString menuHopOnActivate() const { return lookup("menuHopOnActivate"); }
    QString menuHopOnActivateTop() const { return lookup("menuHopOnActivateTop"); }
    QString menuHopOnRelearn() const { return lookup("menuHopOnRelearn"); }
    QString menuHopOnDisable() const { return lookup("menuHopOnDisable"); }
    QString hopOnLearnTitle() const { return lookup("hopOnLearnTitle"); }
    QString hopOnLearnHint() const { return lookup("hopOnLearnHint"); }
    QString hopOnLockedTitle() const { return lookup("hopOnLockedTitle"); }
    QString hopOnLockedHint() const { return lookup("hopOnLockedHint"); }
    QString hopOnSavedToast() const { return lookup("hopOnSavedToast"); }
    QString hopOnAbortedToast() const { return lookup("hopOnAbortedToast"); }
    QString menuSwitchToCluster() const { return lookup("menuSwitchToCluster"); }
    QString menuSwitchToMap() const { return lookup("menuSwitchToMap"); }
    QString menuEnterDestinationCode() const { return lookup("menuEnterDestinationCode"); }
    QString menuDeleteLocation() const { return lookup("menuDeleteLocation"); }
    QString menuStartNavigation() const { return lookup("menuStartNavigation"); }
    QString menuStopNavigation() const { return lookup("menuStopNavigation"); }

    QString optAlways() const { return lookup("optAlways"); }
    QString optActiveOrError() const { return lookup("optActiveOrError"); }
    QString optErrorOnly() const { return lookup("optErrorOnly"); }
    QString optNever() const { return lookup("optNever"); }

    QString controlBack() const { return lookup("controlBack"); }
    QString controlExit() const { return lookup("controlExit"); }
    QString controlSelect() const { return lookup("controlSelect"); }
    QString controlNext() const { return lookup("controlNext"); }
    QString controlScroll() const { return lookup("controlScroll"); }
    QString controlCancel() const { return lookup("controlCancel"); }
    QString navGo() const { return lookup("navGo"); }
    QString controlLeftBrake() const { return lookup("controlLeftBrake"); }
    QString controlRightBrake() const { return lookup("controlRightBrake"); }
    QString controlNextItem() const { return lookup("controlNextItem"); }
    QString controlPressRightBrakeConfirm() const { return lookup("controlPressRightBrakeConfirm"); }
    QString controlPressLeftBrakeEdit() const { return lookup("controlPressLeftBrakeEdit"); }

    QString shuttingDown() const { return lookup("shuttingDown"); }
    QString shutdownComplete() const { return lookup("shutdownComplete"); }
    QString shutdownSuspending() const { return lookup("shutdownSuspending"); }
    QString shutdownHibernationImminent() const { return lookup("shutdownHibernationImminent"); }
    QString shutdownSuspensionImminent() const { return lookup("shutdownSuspensionImminent"); }
    QString shutdownProcessing() const { return lookup("shutdownProcessing"); }
    QString connectingTitle() const { return lookup("connectingTitle"); }
    QString connectingExplanation() const { return lookup("connectingExplanation"); }
    QString connectingBypassHint() const { return lookup("connectingBypassHint"); }
    QString connectionLost() const { return lookup("connectionLost"); }
    QString connectionReconnecting() const { return lookup("connectionReconnecting"); }
    QString connectionRestored() const { return lookup("connectionRestored"); }

    QString umsPreparing() const { return lookup("umsPreparing"); }
    QString umsActive() const { return lookup("umsActive"); }
    QString umsConnect() const { return lookup("umsConnect"); }
    QString umsProcessing() const { return lookup("umsProcessing"); }
    QString umsTitle() const { return lookup("umsTitle"); }
    QString umsConnectToComputer() const { return lookup("umsConnectToComputer"); }
    QString umsHoldExit() const { return lookup("umsHoldExit"); }
    QString controlLeftBrakeHold() const { return lookup("controlLeftBrakeHold"); }

    QString blePinPrompt() const { return lookup("blePinPrompt"); }
    QString bluetoothCommError() const { return lookup("bluetoothCommError"); }
    QString bluetoothPinInstruction() const { return lookup("bluetoothPinInstruction"); }

    QString hibernatePrompt() const { return lookup("hibernatePrompt"); }
    QString hibernateTapKeycard() const { return lookup("hibernateTapKeycard"); }
    QString hibernateSeatboxOpen() const { return lookup("hibernateSeatboxOpen"); }
    QString hibernateCloseSeatbox() const { return lookup("hibernateCloseSeatbox"); }
    QString hibernating() const { return lookup("hibernating"); }
    QString hibernationTitle() const { return lookup("hibernationTitle"); }
    QString hibernationTapKeycardToConfirm() const { return lookup("hibernationTapKeycardToConfirm"); }
    QString hibernationKeepHoldingBrakes() const { return lookup("hibernationKeepHoldingBrakes"); }
    QString hibernationOrHoldBrakes() const { return lookup("hibernationOrHoldBrakes"); }
    QString autoLockTitle() const { return lookup("autoLockTitle"); }
    QString autoLockCancelHint() const { return lookup("autoLockCancelHint"); }
    QString hibernationCancel() const { return lookup("hibernationCancel"); }
    QString hibernationKickstand() const { return lookup("hibernationKickstand"); }
    QString hibernationConfirm() const { return lookup("hibernationConfirm"); }

    QString aboutTitle() const { return lookup("aboutTitle"); }
    QString aboutNonCommercialTitle() const { return lookup("aboutNonCommercialTitle"); }
    QString aboutFossDescription() const { return lookup("aboutFossDescription"); }
    QString aboutOpenSourceComponents() const { return lookup("aboutOpenSourceComponents"); }
    QString aboutScrollAction() const { return lookup("aboutScrollAction"); }
    QString aboutBackAction() const { return lookup("aboutBackAction"); }
    QString aboutBootThemeRestored() const { return lookup("aboutBootThemeRestored"); }
    QString aboutGenuineAdvantage() const { return lookup("aboutGenuineAdvantage"); }
    QString nonCommercialLicense() const { return lookup("nonCommercialLicense"); }
    QString aboutCommercialProhibited() const { return lookup("aboutCommercialProhibited"); }
    QString aboutScamWarning() const { return lookup("aboutScamWarning"); }
    QString aboutAuthorizedPartners() const { return lookup("aboutAuthorizedPartners"); }
    QString aboutSpecialThanks() const { return lookup("aboutSpecialThanks"); }
    QString aboutPatienceNote() const { return lookup("aboutPatienceNote"); }

    QString lowTempWarning() const { return lookup("lowTempWarning"); }
    QString lowTempMotor() const { return lookup("lowTempMotor"); }
    QString lowTempBattery() const { return lookup("lowTempBattery"); }
    QString lowTemp12vBattery() const { return lookup("lowTemp12vBattery"); }
    QString bleCommError() const { return lookup("bleCommError"); }
    QString redisDisconnected() const { return lookup("redisDisconnected"); }
    QString usbDisconnected() const { return lookup("usbDisconnected"); }
    QString locationSaved() const { return lookup("locationSaved"); }
    QString locationDeleted() const { return lookup("locationDeleted"); }
    QString maxLocationsReached() const { return lookup("maxLocationsReached"); }
    QString savedLocationsFailed() const { return lookup("savedLocationsFailed"); }
    QString menuSavedLocations() const { return lookup("menuSavedLocations"); }
    QString menuSaveLocation() const { return lookup("menuSaveLocation"); }
    QString menuNavSetup() const { return lookup("menuNavSetup"); }
    QString plymouthToggled() const { return lookup("plymouthToggled"); }

    QString navCalculating() const { return lookup("navCalculating"); }
    QString navRecalculating() const { return lookup("navRecalculating"); }
    QString navArrived() const { return lookup("navArrived"); }
    QString navOffRoute() const { return lookup("navOffRoute"); }
    QString navSetDestination() const { return lookup("navSetDestination"); }
    QString navUnavailable() const { return lookup("navUnavailable"); }
    QString navEnterCode() const { return lookup("navEnterCode"); }
    QString navConfirmDest() const { return lookup("navConfirmDest"); }
    QString navThen() const { return lookup("navThen"); }
    QString navYouHaveArrived() const { return lookup("navYouHaveArrived"); }
    QString navDistance() const { return lookup("navDistance"); }
    QString navRemaining() const { return lookup("navRemaining"); }
    QString navEta() const { return lookup("navEta"); }
    QString navContinue() const { return lookup("navContinue"); }
    QString navReturnToRoute() const { return lookup("navReturnToRoute"); }
    QString navCurrentPositionNotAvailable() const { return lookup("navCurrentPositionNotAvailable"); }
    QString navCouldNotCalculateRoute() const { return lookup("navCouldNotCalculateRoute"); }
    QString navDestinationUnreachable() const { return lookup("navDestinationUnreachable"); }
    QString navNewDestination() const { return lookup("navNewDestination"); }
    QString navWaitingForGps() const { return lookup("navWaitingForGps"); }
    QString navWaitingForGpsRoute() const { return lookup("navWaitingForGpsRoute"); }
    QString navResumingNavigation() const { return lookup("navResumingNavigation"); }
    QString navArrivedAtDestination() const { return lookup("navArrivedAtDestination"); }
    QString navOffRouteRerouting() const { return lookup("navOffRouteRerouting"); }
    QString navCouldNotCalculateNewRoute() const { return lookup("navCouldNotCalculateNewRoute"); }
    QString navRouteError() const { return lookup("navRouteError"); }

    // Nav short instructions
    QString navShortContinueStraight() const { return lookup("navShortContinueStraight"); }
    QString navShortTurnLeft() const { return lookup("navShortTurnLeft"); }
    QString navShortTurnRight() const { return lookup("navShortTurnRight"); }
    QString navShortTurnSlightlyLeft() const { return lookup("navShortTurnSlightlyLeft"); }
    QString navShortTurnSlightlyRight() const { return lookup("navShortTurnSlightlyRight"); }
    QString navShortTurnSharplyLeft() const { return lookup("navShortTurnSharplyLeft"); }
    QString navShortTurnSharplyRight() const { return lookup("navShortTurnSharplyRight"); }
    QString navShortUturn() const { return lookup("navShortUturn"); }
    QString navShortUturnRight() const { return lookup("navShortUturnRight"); }
    QString navShortMerge() const { return lookup("navShortMerge"); }
    QString navShortMergeLeft() const { return lookup("navShortMergeLeft"); }
    QString navShortMergeRight() const { return lookup("navShortMergeRight"); }
    QString navShortContinue() const { return lookup("navShortContinue"); }

    // Nav setup screen
    QString navSetupTitle() const { return lookup("navSetupTitle"); }
    QString navSetupTitleRoutingUnavailable() const { return lookup("navSetupTitleRoutingUnavailable"); }
    QString navSetupTitleMapsUnavailable() const { return lookup("navSetupTitleMapsUnavailable"); }
    QString navSetupTitleBothUnavailable() const { return lookup("navSetupTitleBothUnavailable"); }
    QString navSetupLocalDisplayMaps() const { return lookup("navSetupLocalDisplayMaps"); }
    QString navSetupRoutingEngine() const { return lookup("navSetupRoutingEngine"); }
    QString navSetupNoRoutingBody() const { return lookup("navSetupNoRoutingBody"); }
    QString navSetupScanForInstructions() const { return lookup("navSetupScanForInstructions"); }
    QString navSetupAllSet() const { return lookup("navSetupAllSet"); }
    QString navSetupDisplayMapsBody() const { return lookup("navSetupDisplayMapsBody"); }
    QString navSetupRoutingBody() const { return lookup("navSetupRoutingBody"); }
    QString navSetupCheckingUpdates() const { return lookup("navSetupCheckingUpdates"); }
    QString navSetupDownloadLocating() const { return lookup("navSetupDownloadLocating"); }
    QString navSetupDownloadInstalling() const { return lookup("navSetupDownloadInstalling"); }
    QString navSetupDownloadDone() const { return lookup("navSetupDownloadDone"); }
    QString navSetupDownloadButton() const { return lookup("navSetupDownloadButton"); }
    QString navSetupUpdateButton() const { return lookup("navSetupUpdateButton"); }
    QString navSetupResumeButton() const { return lookup("navSetupResumeButton"); }
    QString navSetupDownloadError() const { return lookup("navSetupDownloadError"); }
    QString navSetupDownloadUnsupported() const { return lookup("navSetupDownloadUnsupported"); }
    QString navSetupInsufficientSpace() const { return lookup("navSetupInsufficientSpace"); }
    QString navSetupDownloadNoInternet() const { return lookup("navSetupDownloadNoInternet"); }
    QString navSetupDownloadWaitingGps() const { return lookup("navSetupDownloadWaitingGps"); }
    QString navSetupDownloadProgress() const { return lookup("navSetupDownloadProgress"); }
    QString navSetupDownloadProgressBytes() const { return lookup("navSetupDownloadProgressBytes"); }
    QString mapUpdateAvailableToast() const { return lookup("mapUpdateAvailableToast"); }
    QString mapUpdateBadge() const { return lookup("mapUpdateBadge"); }
    QString menuSetupMapMode() const { return lookup("menuSetupMapMode"); }
    QString menuSetupNavigation() const { return lookup("menuSetupNavigation"); }

    // OTA
    QString otaDownloading() const { return lookup("otaDownloading"); }
    QString otaInstalling() const { return lookup("otaInstalling"); }
    QString otaInitializing() const { return lookup("otaInitializing"); }
    QString otaCheckingUpdates() const { return lookup("otaCheckingUpdates"); }
    QString otaCheckFailed() const { return lookup("otaCheckFailed"); }
    QString otaDeviceUpdated() const { return lookup("otaDeviceUpdated"); }
    QString otaWaitingDashboard() const { return lookup("otaWaitingDashboard"); }
    QString otaDownloadingUpdates() const { return lookup("otaDownloadingUpdates"); }
    QString otaDownloadFailed() const { return lookup("otaDownloadFailed"); }
    QString otaInstallingUpdates() const { return lookup("otaInstallingUpdates"); }
    QString otaInstallFailed() const { return lookup("otaInstallFailed"); }
    QString otaWaitingForReboot() const { return lookup("otaWaitingForReboot"); }
    QString otaStatusWaitingForReboot() const { return lookup("otaStatusWaitingForReboot"); }
    QString otaStatusDownloading() const { return lookup("otaStatusDownloading"); }
    QString otaStatusInstalling() const { return lookup("otaStatusInstalling"); }
    QString otaUpdate() const { return lookup("otaUpdate"); }
    QString otaInvalidRelease() const { return lookup("otaInvalidRelease"); }
    QString otaDownloadFailedShort() const { return lookup("otaDownloadFailedShort"); }
    QString otaInstallFailedShort() const { return lookup("otaInstallFailedShort"); }
    QString otaRebootFailed() const { return lookup("otaRebootFailed"); }
    QString otaUpdateError() const { return lookup("otaUpdateError"); }
    QString otaPreparingUpdate() const { return lookup("otaPreparingUpdate"); }
    QString otaPendingReboot() const { return lookup("otaPendingReboot"); }
    QString otaScooterWillTurnOff() const { return lookup("otaScooterWillTurnOff"); }

    // Battery messages
    QString batteryKm() const { return lookup("batteryKm"); }
    QString batteryCbNotCharging() const { return lookup("batteryCbNotCharging"); }
    QString batteryAuxLowNotCharging() const { return lookup("batteryAuxLowNotCharging"); }
    QString batteryAuxVoltageLow() const { return lookup("batteryAuxVoltageLow"); }
    QString batteryAuxVoltageVeryLowReplace() const { return lookup("batteryAuxVoltageVeryLowReplace"); }
    QString batteryAuxVoltageVeryLowCharge() const { return lookup("batteryAuxVoltageVeryLowCharge"); }
    QString batteryEmptyRecharge() const { return lookup("batteryEmptyRecharge"); }
    QString batteryMaxSpeedReduced() const { return lookup("batteryMaxSpeedReduced"); }
    QString batteryLowPowerReduced() const { return lookup("batteryLowPowerReduced"); }
    QString batterySlot0() const { return lookup("batterySlot0"); }
    QString batterySlot1() const { return lookup("batterySlot1"); }

    // Warnings
    QString warningHandlebarLocked() const { return lookup("warningHandlebarLocked"); }
    QString warningLowTemperature() const { return lookup("warningLowTemperature"); }

    // Speed & power
    QString speedKmh() const { return lookup("speedKmh"); }
    QString powerRegen() const { return lookup("powerRegen"); }
    QString powerDischarge() const { return lookup("powerDischarge"); }

    // Fault codes
    QString faultSignalWireBroken() const { return lookup("faultSignalWireBroken"); }
    QString faultCriticalOverTemp() const { return lookup("faultCriticalOverTemp"); }
    QString faultShortCircuit() const { return lookup("faultShortCircuit"); }
    QString faultBmsNotFollowing() const { return lookup("faultBmsNotFollowing"); }
    QString faultBmsCommError() const { return lookup("faultBmsCommError"); }
    QString faultNfcReaderError() const { return lookup("faultNfcReaderError"); }
    QString faultOverTempCharging() const { return lookup("faultOverTempCharging"); }
    QString faultUnderTempCharging() const { return lookup("faultUnderTempCharging"); }
    QString faultOverTempDischarging() const { return lookup("faultOverTempDischarging"); }
    QString faultUnderTempDischarging() const { return lookup("faultUnderTempDischarging"); }
    QString faultMosfetOverTemp() const { return lookup("faultMosfetOverTemp"); }
    QString faultCellOverVoltage() const { return lookup("faultCellOverVoltage"); }
    QString faultCellUnderVoltage() const { return lookup("faultCellUnderVoltage"); }
    QString faultOverCurrentCharging() const { return lookup("faultOverCurrentCharging"); }
    QString faultOverCurrentDischarging() const { return lookup("faultOverCurrentDischarging"); }
    QString faultPackOverVoltage() const { return lookup("faultPackOverVoltage"); }
    QString faultPackUnderVoltage() const { return lookup("faultPackUnderVoltage"); }
    QString faultReserved() const { return lookup("faultReserved"); }
    QString faultBmsZeroData() const { return lookup("faultBmsZeroData"); }
    QString faultUnknown() const { return lookup("faultUnknown"); }
    QString faultMultipleCritical() const { return lookup("faultMultipleCritical"); }
    QString faultMultipleBattery() const { return lookup("faultMultipleBattery"); }

    // ECU fault codes
    QString faultEcuBatteryOverVoltage() const { return lookup("faultEcuBatteryOverVoltage"); }
    QString faultEcuBatteryUnderVoltage() const { return lookup("faultEcuBatteryUnderVoltage"); }
    QString faultEcuMotorShortCircuit() const { return lookup("faultEcuMotorShortCircuit"); }
    QString faultEcuMotorStalled() const { return lookup("faultEcuMotorStalled"); }
    QString faultEcuHallSensor() const { return lookup("faultEcuHallSensor"); }
    QString faultEcuMosfet() const { return lookup("faultEcuMosfet"); }
    QString faultEcuMotorOpenCircuit() const { return lookup("faultEcuMotorOpenCircuit"); }
    QString faultEcuSelfCheck() const { return lookup("faultEcuSelfCheck"); }
    QString faultEcuOverTemperature() const { return lookup("faultEcuOverTemperature"); }
    QString faultEcuThrottleAbnormal() const { return lookup("faultEcuThrottleAbnormal"); }
    QString faultEcuMotorTemperature() const { return lookup("faultEcuMotorTemperature"); }
    QString faultEcuThrottleAtPowerUp() const { return lookup("faultEcuThrottleAtPowerUp"); }
    QString faultEcuInternal15v() const { return lookup("faultEcuInternal15v"); }
    QString faultEcuCommLost() const { return lookup("faultEcuCommLost"); }

    // Status bar & odometer
    QString statusBarDuration() const { return lookup("statusBarDuration"); }
    QString statusBarAvgSpeed() const { return lookup("statusBarAvgSpeed"); }
    QString statusBarTrip() const { return lookup("statusBarTrip"); }
    QString statusBarTotal() const { return lookup("statusBarTotal"); }
    QString statusBarKmh() const { return lookup("statusBarKmh"); }
    QString odometerTrip() const { return lookup("odometerTrip"); }
    QString odometerTotal() const { return lookup("odometerTotal"); }
    QString odometerAvgSpeed() const { return lookup("odometerAvgSpeed"); }
    QString odometerTripTime() const { return lookup("odometerTripTime"); }

    // Address entry
    QString navEnterCity() const { return lookup("navEnterCity"); }
    QString navSelectCity() const { return lookup("navSelectCity"); }
    QString navEnterStreet() const { return lookup("navEnterStreet"); }
    QString navSelectStreet() const { return lookup("navSelectStreet"); }
    QString navSelectNumber() const { return lookup("navSelectNumber"); }
    QString navConfirmDestination() const { return lookup("navConfirmDestination"); }
    QString navCities() const { return lookup("navCities"); }
    QString navStreets() const { return lookup("navStreets"); }
    QString navNoMatches() const { return lookup("navNoMatches"); }
    QString addressLoading() const { return lookup("addressLoading"); }
    QString addressMapNotFound() const { return lookup("addressMapNotFound"); }

    // Standby
    QString standbyWarning() const { return lookup("standbyWarning"); }
    QString standbySeconds() const { return lookup("standbySeconds"); }
    QString standbyCancel() const { return lookup("standbyCancel"); }

    // Destination & map
    QString destinationOfflineOnly() const { return lookup("destinationOfflineOnly"); }
    QString destinationInstallMapData() const { return lookup("destinationInstallMapData"); }
    QString mapWaitingForGps() const { return lookup("mapWaitingForGps"); }
    QString mapOutOfCoverage() const { return lookup("mapOutOfCoverage"); }

    // Shortcut menu
    QString shortcutPressToConfirm() const { return lookup("shortcutPressToConfirm"); }

signals:
    void languageChanged();

private:
    QString lookup(const char *key) const;
    void initStrings();

    QString m_language = QStringLiteral("en");
    QHash<QString, QHash<QString, QString>> m_strings;
};

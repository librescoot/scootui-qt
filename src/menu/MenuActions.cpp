// The single interpreter for MenuAction verbs. MenuDefinition.cpp names what
// a row does; this file is the only place the menu mutates services or issues
// commands, so the menu's entire outbound surface reads top to bottom here.

#include "MenuController.h"
#include "MenuAction.h"

#include "stores/SettingsStore.h"
#include "stores/VehicleStore.h"
#include "stores/SavedLocationsStore.h"
#include "stores/RecentDestinationsStore.h"
#include "stores/InternetStore.h"
#include "core/Navigator.h"
#include "services/HopOnService.h"
#include "services/SettingsService.h"
#include "services/NavigationService.h"
#include "services/MapDownloadService.h"
#include "services/UpdateChannelService.h"
#include "services/ToastService.h"
#include "l10n/Translations.h"
#include "commands/CommandBus.h"
#include "models/Enums.h"

#include <QDebug>
#include <QProcess>

void MenuController::runAction(const MenuAction &action)
{
    using Verb = MenuAction::Verb;

    switch (action.verb) {
    case Verb::None:
        break;

    case Verb::CloseMenu:
        close();
        break;

    case Verb::LockVehicle:
        m_commands->lockVehicle();
        close();
        break;

    case Verb::ToggleHazards:
        m_commands->toggleHazards(m_vehicle->blinkerState());
        close();
        break;

    case Verb::EnableServiceMode:
        m_commands->applyServiceOverlay();
        close();
        break;

    case Verb::DisableServiceMode:
        m_commands->clearServiceOverlay();
        close();
        break;

    case Verb::ClearBluetoothBonds:
        m_commands->deleteAllBluetoothBonds();
        close();
        break;

    case Verb::ShowScreen: {
        const QString screen = action.key;
        closeForScreen();
        if (!m_navigator)
            break;
        if (screen == QLatin1String("address-selection"))
            m_navigator->showAddressSelection();
        else if (screen == QLatin1String("faults"))
            m_navigator->showFaults();
        else if (screen == QLatin1String("about"))
            m_navigator->showAbout();
        else if (screen == QLatin1String("hop-on-info"))
            m_navigator->showHopOnInfo();
        else if (screen == QLatin1String("update-mode-info"))
            m_navigator->showUpdateModeInfo();
        else
            qWarning() << "MenuActions: unknown screen" << screen;
        break;
    }

    case Verb::ShowNavigationSetup:
        closeForScreen();
        if (m_navigator)
            m_navigator->showNavigationSetup(action.value.toInt());
        break;

    case Verb::ShowSystemInfo:
        closeForScreen();
        if (m_navigator)
            m_navigator->showSystemInfo(action.value.toInt());
        break;

    case Verb::SwitchView: {
        const bool toMap = action.key == QLatin1String("map");
        if (m_navigator)
            m_navigator->setScreen(static_cast<int>(toMap ? ScootEnums::ScreenMode::Map
                                                          : ScootEnums::ScreenMode::Cluster));
        m_settingsService->updateMode(toMap ? QStringLiteral("navigation")
                                            : QStringLiteral("speedometer"));
        close();
        break;
    }

    case Verb::HopOnActivate:
        close();
        if (m_hopOn) m_hopOn->activate();
        break;

    case Verb::HopOnRelearn:
        close();
        if (m_hopOn) m_hopOn->startLearning();
        break;

    case Verb::HopOnDisable:
        if (m_hopOn) m_hopOn->disable();
        close();
        break;

    case Verb::StopNavigation:
        if (m_navigationService) m_navigationService->clearNavigation();
        close();
        break;

    case Verb::StartRecent:
        if (m_recentDestinations)
            m_recentDestinations->navigateToRecent(action.value.toInt());
        close();
        break;

    case Verb::SaveRecent:
        if (m_recentDestinations)
            m_recentDestinations->promoteToSaved(action.value.toInt());
        break;

    case Verb::DeleteRecent:
        if (m_recentDestinations)
            m_recentDestinations->deleteRecent(action.value.toInt());
        break;

    case Verb::SaveCurrentLocation:
        if (m_savedLocations)
            m_savedLocations->saveCurrentLocation();
        close();
        break;

    case Verb::StartSaved:
        if (m_savedLocations)
            m_savedLocations->navigateToLocation(action.value.toInt());
        close();
        break;

    case Verb::DeleteSaved:
        if (m_savedLocations)
            m_savedLocations->deleteLocation(action.value.toInt());
        break;

    case Verb::SetSetting:
        applySetting(action.key, action.value);
        break;

    case Verb::ToggleSetting:
        toggleSetting(action.key);
        break;

    // The automatic map-update check runs at most weekly; this asks now and
    // reports the outcome once, for this check only: the automatic path stays
    // silent when nothing changed.
    case Verb::CheckMapUpdates: {
        if (!m_internet || m_internet->modemState()
                != static_cast<int>(ScootEnums::ModemState::Connected)) {
            if (m_toastService)
                m_toastService->showError(m_translations->navSetupDownloadNoInternet());
            close();
            break;
        }

        auto *conn = new QMetaObject::Connection;
        *conn = connect(m_mapDownload, &MapDownloadService::updateCheckCompleted,
                        this, [this, conn](bool updateFound) {
            disconnect(*conn);
            delete conn;
            if (!m_toastService)
                return;
            if (updateFound)
                m_toastService->showInfo(m_translations->mapUpdateAvailableToast());
            else
                m_toastService->showSuccess(m_translations->mapsUpToDateToast());
        });

        if (m_toastService)
            m_toastService->showInfo(m_translations->mapCheckingToast());
        m_mapDownload->checkForUpdatesNow();
        close();
        break;
    }

    case Verb::CheckOsUpdates:
        if (m_toastService)
            m_toastService->showInfo(m_translations->updateCheckStartedToast());
        m_settingsService->triggerUpdateCheck();
        close();
        break;

    case Verb::SetOtaMethod: {
        const QString value = action.value.toString();
        const bool split = m_settings->otaMethodDiverged();
        const QString current = split ? QString()
                              : (m_settings->otaMethod() == QLatin1String("full")
                                     ? QStringLiteral("full") : QStringLiteral("delta"));
        if (value != current)
            m_settingsService->updateOtaMethod(value);
        goBack();
        break;
    }

    case Verb::SwitchUpdateChannel: {
        const QString value = action.value.toString();
        const bool split = m_settings->otaChannelDiverged();
        const QString current = (split || !m_updateChannel) ? QString()
                                                            : m_updateChannel->currentChannel();
        if (value == current) {
            goBack();
            break;
        }
        if (!m_updateChannel)
            break;
        if (m_updateChannel->isUpdateInProgress()) {
            if (m_toastService)
                m_toastService->showError(m_translations->updateCheckBusyToast());
            close();
            break;
        }
        m_updateChannel->beginSwitch(value);
        closeForScreen();
        if (m_navigator)
            m_navigator->showUpdateChannel();
        break;
    }

    // `ssh mdb lsc logs`. Closes the menu immediately and runs the bundle job
    // in the background. A toast confirms it kicked off, then a second toast
    // reports success or failure when ssh exits. Assumes DBC->MDB key-based
    // ssh is set up by the image build; bundle lands in
    // /data/logs-<timestamp>.tar.gz on MDB.
    case Verb::CaptureLogs: {
        auto *proc = new QProcess(this);
        proc->setProgram(QStringLiteral("ssh"));
        proc->setArguments({QStringLiteral("-y"), QStringLiteral("mdb"),
                            QStringLiteral("lsc"), QStringLiteral("logs")});

        connect(proc, &QProcess::errorOccurred, this,
                [this, proc](QProcess::ProcessError err) {
            qWarning() << "[MenuActions] Capture Logs ssh errorOccurred:" << err;
            if (m_toastService)
                m_toastService->showError(m_translations->captureLogsToastFailed());
            proc->deleteLater();
        });

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc](int exitCode, QProcess::ExitStatus status) {
            qInfo() << "[MenuActions] Capture Logs ssh finished, exitCode:" << exitCode
                    << "status:" << status;
            if (m_toastService) {
                if (status == QProcess::NormalExit && exitCode == 0)
                    m_toastService->showSuccess(m_translations->captureLogsToastDone());
                else
                    m_toastService->showError(m_translations->captureLogsToastFailed());
            }
            proc->deleteLater();
        });

        proc->start();
        qInfo() << "[MenuActions] Capture Logs triggered";
        if (m_toastService)
            m_toastService->showInfo(m_translations->captureLogsToastStarted());
        close();
        break;
    }
    }
}

// Maps a setting id from the tree onto the SettingsService method that writes
// it. One compound entry: "map-updates" folds the check and auto-download
// flags into a single three-state control (see the definition).
void MenuController::applySetting(const QString &key, const QVariant &value)
{
    auto *svc = m_settingsService;
    const QString v = value.toString();

    if (key == QLatin1String("auto-theme"))
        svc->updateAutoTheme(value.toBool());
    else if (key == QLatin1String("theme"))
        svc->updateTheme(v);
    else if (key == QLatin1String("backlight-mode"))
        svc->updateBacklightMode(v);
    else if (key == QLatin1String("power-display-mode"))
        svc->updatePowerDisplayMode(v);
    else if (key == QLatin1String("battery-display-mode"))
        svc->updateBatteryDisplayMode(v);
    else if (key == QLatin1String("show-cb-battery"))
        svc->updateShowCbBattery(v);
    else if (key == QLatin1String("show-aux-battery"))
        svc->updateShowAuxBattery(v);
    else if (key == QLatin1String("show-gps"))
        svc->updateShowGps(v);
    else if (key == QLatin1String("show-bluetooth"))
        svc->updateShowBluetooth(v);
    else if (key == QLatin1String("show-cloud"))
        svc->updateShowCloud(v);
    else if (key == QLatin1String("show-internet"))
        svc->updateShowInternet(v);
    else if (key == QLatin1String("show-clock"))
        svc->updateShowClock(v);
    else if (key == QLatin1String("show-temperature"))
        svc->updateShowTemperature(v);
    else if (key == QLatin1String("map-view-mode"))
        svc->updateMapViewMode(v);
    else if (key == QLatin1String("map-north-oriented"))
        svc->updateMapNorthOriented(value.toBool());
    else if (key == QLatin1String("route-preference"))
        svc->updateRoutePreference(v);
    else if (key == QLatin1String("avoid-cobblestone"))
        svc->updateAvoidCobblestone(v);
    else if (key == QLatin1String("map-updates")) {
        svc->updateMapCheckForUpdates(v != QLatin1String("off"));
        svc->updateMapAutoDownload(v == QLatin1String("download"));
    }
    else if (key == QLatin1String("map-type"))
        svc->updateMapType(v);
    else if (key == QLatin1String("valhalla-url"))
        svc->updateValhallaEndpoint(v);
    else if (key == QLatin1String("blinker-style"))
        svc->updateBlinkerStyle(v);
    else if (key == QLatin1String("alarm-duration"))
        svc->updateAlarmDuration(value.toInt());
    else if (key == QLatin1String("language"))
        svc->updateLanguage(v);
    else if (key == QLatin1String("dual-battery"))
        svc->updateDualBattery(value.toBool());
    else if (key == QLatin1String("ota-check-interval"))
        svc->updateOtaCheckInterval(v);
    else
        qWarning() << "MenuActions: unknown setting" << key;
}

// Toggles read the current value live rather than baking it into the tree at
// build time, so a value that changed since the last rebuild still flips.
void MenuController::toggleSetting(const QString &key)
{
    auto *svc = m_settingsService;

    if (key == QLatin1String("dbc-blinker-led"))
        svc->updateDbcBlinkerLed(!m_settings->dbcBlinkerLed());
    else if (key == QLatin1String("milestone-celebrations"))
        svc->updateMilestoneCelebrations(!m_settings->milestoneCelebrations());
    else if (key == QLatin1String("alarm-enabled"))
        svc->updateAlarmEnabled(!m_settings->alarmEnabled());
    else if (key == QLatin1String("alarm-honk"))
        svc->updateAlarmHonk(!m_settings->alarmHonk());
    else if (key == QLatin1String("horn-when-seatbox-open"))
        svc->updateHornWhenSeatboxOpen(!m_settings->hornWhenSeatboxOpen());
    else
        qWarning() << "MenuActions: unknown toggle" << key;
}

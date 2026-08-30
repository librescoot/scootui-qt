#include "MapUpdateCoordinator.h"

#include "commands/CommandBus.h"
#include "l10n/Translations.h"
#include "models/Enums.h"
#include "repositories/MdbRepository.h"
#include "repositories/RedisSchema.h"
#include "services/AddressDatabaseService.h"
#include "services/MapDownloadService.h"
#include "services/MapService.h"
#include "services/NavigationAvailabilityService.h"
#include "services/RoadInfoService.h"
#include "services/ToastService.h"
#include "stores/GpsStore.h"
#include "stores/InternetStore.h"
#include "stores/SettingsStore.h"
#include "stores/VehicleStore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <iterator>

MapUpdateCoordinator::MapUpdateCoordinator(const Deps &deps, QObject *parent)
    : QObject(parent)
    , m_d(deps)
{
    // Vehicles whose maps came from the flasher have no region on record. The
    // update check identifies them by tile digest where it can; where it
    // can't, a GPS fix gives it something to resolve from.
    auto *gps = m_d.gps;
    m_d.download->setPositionProvider([gps](double &lat, double &lng) {
        if (!gps->hasRecentFix())
            return false;
        lat = gps->latitude();
        lng = gps->longitude();
        return true;
    });

    // The check needs both the modem connected and the setting loaded, and the
    // two stores sync independently. Listening to only one of them loses the
    // race whenever the other lands second: internetStore starts before
    // settingsStore, so the initial modemStateChanged used to arrive while
    // mapCheckForUpdates was still its default "false", and modem-state never
    // changes again on a vehicle that stays online. Hooking both signals
    // means whichever settles last runs the check, in either order.
    connect(m_d.internet, &InternetStore::modemStateChanged, this,
            &MapUpdateCoordinator::maybeCheckForMapUpdates);
    connect(m_d.settings, &SettingsStore::mapCheckForUpdatesChanged, this,
            &MapUpdateCoordinator::maybeCheckForMapUpdates);

    // Notify user when a map update is found, or auto-download if enabled.
    // The actual download is state-gated (see maybeAutoDownloadMaps).
    connect(m_d.download, &MapDownloadService::updateAvailableChanged, this, [this]() {
        if (!m_d.download->updateAvailable())
            return;
        if (m_d.settings->mapAutoDownload()) {
            maybeAutoDownloadMaps();
        } else {
            m_d.toasts->showInfo(m_d.translations->mapUpdateAvailableToast());
        }
    });

    // Retry the auto-download on every state the download is allowed in, so an
    // update discovered at an awkward moment (or already pending from a
    // previous session) starts as soon as the vehicle reaches one of them.
    connect(m_d.vehicle, &VehicleStore::stateChanged, this, [this]() {
        auto state = static_cast<ScootEnums::VehicleState>(m_d.vehicle->state());
        if (state == ScootEnums::VehicleState::Parked
            || state == ScootEnums::VehicleState::StandBy
            || state == ScootEnums::VehicleState::ReadyToDrive) {
            maybeAutoDownloadMaps();
        }
    });

    // Ask vehicle-service to keep DBC power up while a download is running, the
    // way a DBC OTA does. Without it, locking the scooter cuts power mid
    // transfer: the .part file resumes next time, but on a scooter that parks
    // often a large tar may never finish. vehicle-service caps the hold, so a
    // dashboard that dies mid download cannot pin power on.
    connect(m_d.download, &MapDownloadService::statusChanged, this, [this]() {
        const int st = m_d.download->status();
        // Installing is short but it renames the tar into place and restarts
        // valhalla, so it is the worst moment to lose power.
        const bool busy = st == static_cast<int>(ScootEnums::MapDownloadStatus::Downloading)
                          || st == static_cast<int>(ScootEnums::MapDownloadStatus::Installing);
        if (busy == m_downloadHoldActive)
            return;
        m_downloadHoldActive = busy;
        if (busy)
            m_d.commandBus->holdDbc(QStringLiteral("map-download"));
        else
            m_d.commandBus->releaseDbcHold();
    });

    // Refresh map/road-info/address-db as soon as an install finishes.
    // Belt-and-suspenders with the mbtiles file watcher, which reacts to the
    // same rename but only once inotify delivers the event.
    connect(m_d.download, &MapDownloadService::downloadComplete,
            this, &MapUpdateCoordinator::reloadMapServices);

    // Show persisted update notification on startup while parked/stand-by
    if (m_d.download->updateAvailable()) {
        auto *startupConn = new QMetaObject::Connection;
        *startupConn = connect(m_d.vehicle, &VehicleStore::stateChanged, this,
                [this, startupConn]() {
            auto state = static_cast<ScootEnums::VehicleState>(m_d.vehicle->state());
            if (state == ScootEnums::VehicleState::Parked
                || state == ScootEnums::VehicleState::StandBy) {
                if (m_d.download->updateAvailable())
                    m_d.toasts->showInfo(m_d.translations->mapUpdateAvailableToast());
            }
            disconnect(*startupConn);
            delete startupConn;
        });

        // A persisted flag from a previous session never re-fires
        // updateAvailableChanged, so kick the parked-gated auto-download once
        // here too; it self-gates on the current vehicle state and setting.
        maybeAutoDownloadMaps();
    }

    setupMbtilesWatcher();

    // The QFileSystemWatcher cannot see /data being *mounted* over the watched
    // mountpoint (inotify delivers no event for a mount), so on a cold boot
    // where scootui starts before /data is mounted it never fires. The
    // availability poller does detect the late mount — reload the
    // mbtiles-backed services off its edge so the map + road-info recover,
    // not just the flag.
    if (m_d.availability)
        connect(m_d.availability, &NavigationAvailabilityService::localMapsBecameAvailable,
                this, &MapUpdateCoordinator::reloadMapServices);

    setupCommandChannel();
}

void MapUpdateCoordinator::reloadMapServices()
{
    // Each reload is idempotent: MapService/RoadInfoService early-return when
    // the path is unchanged, and AddressDatabaseService skips a rebuild
    // already in flight — so this is safe to call from both the file watcher
    // and the availability edge, including the re-entrant watcher->recheck()
    // case.
    if (m_d.map)
        m_d.map->reloadMbtiles();
    if (m_d.roadInfo)
        m_d.roadInfo->reloadMbtiles();
    if (m_d.addressDb)
        m_d.addressDb->initialize();
}

void MapUpdateCoordinator::maybeCheckForMapUpdates()
{
    if (!m_d.settings->mapCheckForUpdates())
        return;
    if (m_d.internet->modemState() != static_cast<int>(ScootEnums::ModemState::Connected))
        return;
    if (!m_d.download->hasMapsInstalled())
        return;
    if (!m_d.download->shouldCheckForUpdates())
        return;

    qDebug() << "Auto-checking for map updates (weekly)";
    m_d.download->checkForUpdatesNow();
}

void MapUpdateCoordinator::maybeAutoDownloadMaps()
{
    if (!m_d.settings->mapAutoDownload())
        return;
    if (!m_d.download->updateAvailable())
        return;
    if (m_d.download->status() != static_cast<int>(ScootEnums::MapDownloadStatus::Idle))
        return;

    auto state = static_cast<ScootEnums::VehicleState>(m_d.vehicle->state());
    if (state != ScootEnums::VehicleState::Parked
        && state != ScootEnums::VehicleState::StandBy
        && state != ScootEnums::VehicleState::ReadyToDrive)
        return;

    qDebug() << "Auto-downloading map update, vehicle state" << m_d.vehicle->state();
    m_d.download->startDownload(m_d.gps->latitude(), m_d.gps->longitude(), true, true);
}

// Watch /data/maps/ for mbtiles appearing late (e.g. /data not yet mounted at
// startup) or being replaced (e.g. OTA map update). inotify on the mountpoint
// directory fires when the filesystem is mounted.
void MapUpdateCoordinator::setupMbtilesWatcher()
{
    auto *mbtilesWatcher = new QFileSystemWatcher(this);
    static const QString mapsDir = QStringLiteral("/data/maps");
    if (QDir(mapsDir).exists())
        mbtilesWatcher->addPath(mapsDir);
    else
        mbtilesWatcher->addPath(QStringLiteral("/data"));

    connect(mbtilesWatcher, &QFileSystemWatcher::directoryChanged, this,
            [this, mbtilesWatcher](const QString &path) {
        static const QString mapsDir = QStringLiteral("/data/maps");
        // /data was just mounted — start watching /data/maps/ instead
        if (path == QLatin1String("/data") && QDir(mapsDir).exists()) {
            mbtilesWatcher->removePath(QStringLiteral("/data"));
            mbtilesWatcher->addPath(mapsDir);
        }

        // Everything below the mbtiles check needs /data to exist, and both of
        // these were previously stranded when it mounted late. The metadata read
        // in MapDownloadService's constructor happens ~8s before the mount, and
        // the update check only ran off the modem and settings edges, which can
        // both settle while hasMapsInstalled() is still false.
        m_d.download->reloadMetadata();
        maybeCheckForMapUpdates();

        if (!QFile::exists(mapsDir + QStringLiteral("/map.mbtiles")))
            return;
        qDebug() << "Mbtiles change detected, reloading services";
        reloadMapServices();
        // Re-run availability detection immediately. The service also polls
        // while maps are unavailable (covers mounts, which inotify can't see),
        // but reacting to the watcher here flips the flag without waiting for
        // the next poll tick.
        if (m_d.availability)
            m_d.availability->recheck();
    });
}

void MapUpdateCoordinator::setupCommandChannel()
{
    // Map updates could only be started by a human in the settings menu, which
    // made the whole path untestable without standing at the vehicle and
    // impossible to drive from a fleet tool. Commands arrive on the
    // scootui:command channel:
    //
    //   redis-cli publish scootui:command map-check
    //   redis-cli publish scootui:command map-download
    //   redis-cli publish scootui:command map-cancel
    //   redis-cli publish scootui:command map-reload
    //
    m_d.repo->subscribe(RedisSchema::channel::ScootuiCommand,
                        [this](const QString &, const QString &msg) {
        const QString cmd = msg.trimmed();
        if (cmd == QLatin1String("map-check")) {
            qInfo() << "Map: check requested over scootui:command";
            QMetaObject::invokeMethod(m_d.download,
                                      &MapDownloadService::checkForUpdatesNow,
                                      Qt::QueuedConnection);
        } else if (cmd == QLatin1String("map-download")) {
            qInfo() << "Map: download requested over scootui:command";
            QMetaObject::invokeMethod(this, [this] {
                double lat = 0.0, lng = 0.0;
                if (m_d.gps && m_d.gps->latitude() != 0.0) {
                    lat = m_d.gps->latitude();
                    lng = m_d.gps->longitude();
                }
                m_d.download->startDownload(lat, lng, true, true);
            }, Qt::QueuedConnection);
        } else if (cmd == QLatin1String("map-cancel")) {
            qInfo() << "Map: cancel requested over scootui:command";
            QMetaObject::invokeMethod(m_d.download,
                                      &MapDownloadService::cancel,
                                      Qt::QueuedConnection);
        } else if (cmd == QLatin1String("map-reload")) {
            // Swapping map.mbtiles by hand otherwise needs a service restart.
            qInfo() << "Map: reloading mbtiles over scootui:command";
            if (m_d.map) {
                QMetaObject::invokeMethod(m_d.map,
                                          &MapService::reloadMbtiles,
                                          Qt::QueuedConnection);
            }
        } else if (!cmd.isEmpty()) {
            qWarning() << "Map: unknown scootui:command" << cmd;
        }
    });

    // The service only reported progress to the screen, so a check that ran
    // while nobody was looking left no trace. Mirror the interesting
    // transitions to the journal.
    static const char *kStatusNames[] = {
        "idle", "checking-updates", "locating", "downloading",
        "installing", "done", "error"
    };
    connect(m_d.download, &MapDownloadService::statusChanged, this, [this] {
        const int s = m_d.download->status();
        const char *name = (s >= 0 && s < int(std::size(kStatusNames)))
            ? kStatusNames[s] : "unknown";
        qInfo().nospace() << "Map: status " << name
                          << " region=" << m_d.download->regionName();
    });
    connect(m_d.download, &MapDownloadService::updateCheckCompleted, this,
            [](bool updateFound) {
        qInfo() << "Map: update check finished, update available:" << updateFound;
    });
    connect(m_d.download, &MapDownloadService::downloadComplete, this, [this] {
        qInfo() << "Map: download complete, region" << m_d.download->regionName();
    });
    connect(m_d.download, &MapDownloadService::errorMessageChanged, this, [this] {
        const QString err = m_d.download->errorMessage();
        if (!err.isEmpty())
            qWarning() << "Map: error:" << err;
    });
}

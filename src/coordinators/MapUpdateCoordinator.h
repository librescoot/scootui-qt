#pragma once

#include <QObject>

class MdbRepository;
class CommandBus;
class MapDownloadService;
class NavigationAvailabilityService;
class MapService;
class RoadInfoService;
class AddressDatabaseService;
class ToastService;
class Translations;
class GpsStore;
class VehicleStore;
class SettingsStore;
class InternetStore;

// Owns the map update lifecycle around MapDownloadService: the weekly
// auto-check, the parked-gated auto-download, the DBC power hold while a
// download runs, mbtiles reloads when an install (or a late /data mount)
// swaps the file, and the scootui:command remote-control channel.
// Application constructs one and forgets about it.
class MapUpdateCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        MdbRepository *repo = nullptr;
        CommandBus *commandBus = nullptr;
        MapDownloadService *download = nullptr;
        NavigationAvailabilityService *availability = nullptr;
        MapService *map = nullptr;
        RoadInfoService *roadInfo = nullptr;
        AddressDatabaseService *addressDb = nullptr;
        ToastService *toasts = nullptr;
        Translations *translations = nullptr;
        GpsStore *gps = nullptr;
        VehicleStore *vehicle = nullptr;
        SettingsStore *settings = nullptr;
        InternetStore *internet = nullptr;
    };

    explicit MapUpdateCoordinator(const Deps &deps, QObject *parent = nullptr);

private:
    // Re-point the mbtiles-backed services at /data/maps/map.mbtiles. Safe to
    // call repeatedly (each service's reload is idempotent); driven by the
    // file watcher, the download-complete signal, and by
    // NavigationAvailabilityService::localMapsBecameAvailable so a late /data
    // mount recovers the map + road-info, not just the flag.
    void reloadMapServices();
    // Runs the periodic map update check when the setting, connectivity,
    // installed maps and cadence all allow it. Cheap and idempotent, so it is
    // safe to call from every signal that could make those conditions true.
    void maybeCheckForMapUpdates();
    // Starts the map download only while parked/stand-by, so a mid-ride
    // update never triggers a large cellular download or a valhalla restart
    // during navigation. Self-gates on the auto-download setting, update
    // availability, download-service idle status, and vehicle state, so it's
    // safe to call opportunistically from multiple signals.
    void maybeAutoDownloadMaps();
    void setupMbtilesWatcher();
    void setupCommandChannel();

    Deps m_d;
    bool m_downloadHoldActive = false;
};

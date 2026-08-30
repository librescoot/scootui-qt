#pragma once

#include <QObject>

class MdbRepository;
class SerialNumberService;
class MapDownloadService;

// Owns the startup handshake: publishes the dashboard-ready hash on every
// Redis connect, hands the display over from boot-animation once the first
// frame is on screen, and gates systemd's READY=1 on both halves.
class ReadinessCoordinator : public QObject
{
    Q_OBJECT

public:
    ReadinessCoordinator(MdbRepository *repo, SerialNumberService *serialNumber,
                         MapDownloadService *mapDownload, QObject *parent = nullptr);

    // Call once the first frame is actually on screen. Hands the display over
    // from boot-animation and, together with the Redis connect, gates the
    // systemd READY=1. Idempotent.
    void uiPresented();

private:
    // Notify other services that the dashboard is ready. Runs on startup and
    // on every reconnect.
    void publishReady();
    // READY=1 fires once both halves are true: the UI has painted and the
    // Redis worker is connected (so the dashboard hash has really been
    // published). Called from both edges, whichever lands last wins.
    void maybeSignalReady();
    void fadeInOverlay();

    MdbRepository *m_repo;
    SerialNumberService *m_serialNumber;
    MapDownloadService *m_mapDownload;
    bool m_uiPresented = false;
    bool m_redisReady = false;
    bool m_readySignalled = false;
};

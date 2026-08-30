#include "ReadinessCoordinator.h"

#include "repositories/MdbRepository.h"
#include "repositories/RedisSchema.h"
#include "services/MapDownloadService.h"
#include "services/SerialNumberService.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>

// Shared boot timer defined in main.cpp — markers added at startup
// checkpoints so we can see where time goes on a live DBC.
extern QElapsedTimer g_bootTimer;
#define BOOT_MARK(what) \
    qDebug().nospace().noquote() << QStringLiteral("[boot +%1ms] %2").arg(g_bootTimer.elapsed(), 5).arg(QStringLiteral(what))

#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>

static void sdNotifyReady()
{
    const char *sockPath = ::getenv("NOTIFY_SOCKET");
    if (!sockPath) return;

    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return;

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path) - 1);
    if (addr.sun_path[0] == '@')
        addr.sun_path[0] = '\0';

    ::sendto(fd, "READY=1", 7, 0, (struct sockaddr *)&addr,
             offsetof(struct sockaddr_un, sun_path) + ::strlen(sockPath));
    ::close(fd);
}
#endif

ReadinessCoordinator::ReadinessCoordinator(MdbRepository *repo,
                                           SerialNumberService *serialNumber,
                                           MapDownloadService *mapDownload,
                                           QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_serialNumber(serialNumber)
    , m_mapDownload(mapDownload)
{
    connect(m_repo, &MdbRepository::connectionStateChanged, this, [this](bool connected) {
        if (connected)
            publishReady();
    });
    publishReady();
}

void ReadinessCoordinator::publishReady()
{
    if (m_serialNumber->available()) {
        m_repo->set(RedisSchema::hash::Dashboard, QStringLiteral("serial-number"),
                    m_serialNumber->serialNumber());
    }
    if (m_mapDownload)
        m_mapDownload->publishToRedis();
    m_repo->dashboardReady();
    // The first call runs before the worker has connected (the prewarm uses
    // its own throwaway context), so isConnected() is what decides whether
    // the ready publish actually reached Redis.
    m_redisReady = m_repo->isConnected();
    maybeSignalReady();
}

void ReadinessCoordinator::uiPresented()
{
    if (m_uiPresented)
        return;
    m_uiPresented = true;
    fadeInOverlay();
    maybeSignalReady();
}

void ReadinessCoordinator::maybeSignalReady()
{
#ifdef Q_OS_LINUX
    if (m_readySignalled || !m_uiPresented || !m_redisReady)
        return;
    m_readySignalled = true;
    BOOT_MARK("sd_notify READY=1");
    sdNotifyReady();
#endif
}

void ReadinessCoordinator::fadeInOverlay()
{
#ifdef Q_OS_LINUX
    auto stopBootAnimation = []() {
        QProcess::startDetached(QStringLiteral("systemctl"),
                                 {QStringLiteral("stop"), QStringLiteral("boot-animation.service")});
        qDebug() << "Boot animation stopped";
    };

    if (!QFile::exists(QStringLiteral("/sys/class/graphics/fb1/overlay_alpha"))) {
        // No overlay alpha (kernel 6.6 imx-drm) — stop boot-animation directly
        stopBootAnimation();
        return;
    }

    qDebug() << "Starting boot animation fade-in...";
    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("/usr/bin/imx-overlay-alpha"));
    proc->setArguments({QStringLiteral("fade"), QStringLiteral("0"),
                        QStringLiteral("255"), QStringLiteral("1000")});

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [proc, stopBootAnimation](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (exitCode == 0) {
            stopBootAnimation();
        }
    });

    proc->start();
#endif
}

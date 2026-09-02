#pragma once

#include "MdbRepository.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

// Fetches the boot hashes over a private hiredis connection on a plain
// thread, so it can run before QGuiApplication exists. No Qt event loop and
// no signals: waitFinished() and take() are the handoff.
class BootPrefetch
{
public:
    BootPrefetch(QString host, quint16 port, QString backupHost,
                 QStringList channels, int deadlineMs = 10000);
    ~BootPrefetch();

    BootPrefetch(const BootPrefetch &) = delete;
    BootPrefetch &operator=(const BootPrefetch &) = delete;

    void start();
    bool waitFinished(int timeoutMs);
    bool succeeded() const { return m_succeeded.load(); }
    bool usedBackup() const { return m_usedBackup.load(); }
    qint64 finishedAtMs() const { return m_finishedAtMs.load(); }
    QHash<QString, FieldMap> take();

private:
    void run();

    const QString m_host;
    const quint16 m_port;
    const QString m_backupHost;
    const QStringList m_channels;
    const int m_deadlineMs;

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_finished = false;
    QHash<QString, FieldMap> m_result;
    std::atomic<bool> m_cancel{false};
    std::atomic<bool> m_succeeded{false};
    std::atomic<bool> m_usedBackup{false};
    std::atomic<qint64> m_finishedAtMs{-1};
};

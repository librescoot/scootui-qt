#include "SoundCuePlayer.h"
#include <QCoreApplication>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>
#include <array>
#include <atomic>
#include <chrono>
#include <optional>

namespace {
constexpr int CueCount = int(SoundCue::Error) + 1;
bool valid(SoundCue cue) { return int(cue) > 0 && int(cue) < CueCount; }
bool stateCue(SoundCue cue)
{
    return cue == SoundCue::Wake || cue == SoundCue::Ready
        || cue == SoundCue::Parked || cue == SoundCue::Shutdown;
}
qint64 nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
QList<QThread *> &audioThreads()
{
    static QList<QThread *> threads;
    return threads;
}
}

struct SoundCuePlayer::Shared {
    struct Command { SoundCue cue; quint64 sequence; qint64 at; };
    QMutex mutex;
    std::array<std::optional<Command>, CueCount> pending;
    std::atomic<quint32> available{0};
    bool stopped = false;
    bool scheduled = false;
    quint64 sequence = 0;
};

class SoundCuePlayer::Worker : public QObject {
public:
    Worker(std::shared_ptr<Shared> shared, Factory factory)
        : m_shared(std::move(shared)), m_factory(std::move(factory)) {}
    void initialize(const QString &root)
    {
        if (QThread::currentThread()->isInterruptionRequested())
            return;
        m_backend = m_factory();
        if (!m_backend)
            return;
        Q_ASSERT(m_backend->thread() == QThread::currentThread());
        m_backend->setParent(this);
        connect(m_backend, &SoundCueBackend::availableCuesChanged, this,
                [shared = m_shared](quint32 mask) { shared->available.store(mask); });
        m_backend->initialize(root);
    }
    void next()
    {
        std::optional<Shared::Command> command;
        {
            QMutexLocker lock(&m_shared->mutex);
            if (m_shared->stopped) {
                m_shared->scheduled = false;
                return;
            }
            for (const auto &entry : m_shared->pending)
                if (entry && (!command || entry->sequence < command->sequence))
                    command = entry;
            if (!command) {
                m_shared->scheduled = false;
                return;
            }
            m_shared->pending[int(command->cue)].reset();
        }
        // No lock is held across backend calls, which can block in ALSA.
        if (m_backend && (m_shared->available.load() & (1u << int(command->cue)))
            && !QThread::currentThread()->isInterruptionRequested()
            && (command->cue != SoundCue::BlinkerPulse
                || nowMs() - command->at < PulseDeadlineMs))
            m_backend->play(command->cue);
        // Yield between calls; at most one drain callback is ever queued.
        QMetaObject::invokeMethod(this, [this]() { next(); }, Qt::QueuedConnection);
    }
private:
    std::shared_ptr<Shared> m_shared;
    Factory m_factory;
    SoundCueBackend *m_backend = nullptr;
};

SoundCuePlayer::SoundCuePlayer(QString assetRoot, Factory factory, QObject *parent, int loadDelayMs)
    : QObject(parent), m_shared(std::make_shared<Shared>()), m_thread(new QThread),
      m_worker(new Worker(m_shared, std::move(factory)))
{
    Q_ASSERT(QThread::currentThread() == qApp->thread());
    m_thread->setObjectName(QStringLiteral("sound-cues"));
    audioThreads().append(m_thread);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_thread, &QObject::destroyed, [thread = m_thread]() { audioThreads().removeOne(thread); });
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, root = std::move(assetRoot), loadDelayMs]() {
        QTimer::singleShot(loadDelayMs, worker, [worker, root]() { worker->initialize(root); });
    }, Qt::QueuedConnection);
    m_thread->start();
}

SoundCuePlayer::~SoundCuePlayer() { stop(); }

bool SoundCuePlayer::available(SoundCue cue) const
{
    return valid(cue) && (m_shared->available.load() & (1u << int(cue)));
}

void SoundCuePlayer::play(SoundCue cue)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!valid(cue))
        return;
    QMutexLocker lock(&m_shared->mutex);
    if (m_shared->stopped)
        return;
    if (stateCue(cue)) {
        for (int i = 1; i < CueCount; ++i)
            if (stateCue(SoundCue(i)))
                m_shared->pending[i].reset();
    }
    if (cue == SoundCue::BlinkerOff)
        m_shared->pending[int(SoundCue::BlinkerPulse)].reset();
    if (cue == SoundCue::BlinkerPulse)
        m_shared->pending[int(SoundCue::BlinkerOff)].reset();
    // Even an unavailable latest cue supersedes obsolete pending state/edges.
    // Drop the new cue itself until its effect exists; never queue boot history.
    if (!available(cue))
        return;
    m_shared->pending[int(cue)] = Shared::Command{cue, ++m_shared->sequence, nowMs()};
    if (!m_shared->scheduled) {
        m_shared->scheduled = true;
        QMetaObject::invokeMethod(m_worker, [worker = m_worker]() { worker->next(); }, Qt::QueuedConnection);
    }
}

int SoundCuePlayer::pendingCount() const
{
    QMutexLocker lock(&m_shared->mutex);
    int count = 0;
    for (const auto &entry : m_shared->pending)
        count += entry.has_value();
    return count;
}

void SoundCuePlayer::stop()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_stopped)
        return;
    m_stopped = true;
    {
        QMutexLocker lock(&m_shared->mutex);
        m_shared->stopped = true;
        m_shared->available.store(0);
        for (auto &entry : m_shared->pending)
            entry.reset();
    }
    m_thread->requestInterruption();
    m_thread->quit();
}

void SoundCuePlayer::drainAfterEventLoop()
{
    Q_ASSERT(QThread::currentThread() == qApp->thread());
    const auto remaining = audioThreads();
    for (auto *thread : remaining) {
        thread->requestInterruption();
        thread->quit();
    }
    for (auto *thread : remaining) {
        thread->wait();
        delete thread;
    }
}

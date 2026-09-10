#include "RoadMatchDispatcher.h"
#include "RoadWorkerThreads.h"
#include <QDebug>
#include <QThread>

class RoadMatchWorker : public QObject
{
    Q_OBJECT
public:
    explicit RoadMatchWorker(RoadMatchDispatcher::Compute compute)
        : m_compute(std::move(compute)) {}
    void run(const RoadMatchRequest &request)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!QThread::currentThread()->isInterruptionRequested()) {
            const auto result = m_compute(request);
            auto completedRequest = request;
            completedRequest.tiles.clear(); // Release job-owned tile data on the worker.
            emit completed(completedRequest, result);
        }
    }
signals:
    void completed(const RoadMatchRequest &request, const RoadMatchResult &result);
private:
    RoadMatchDispatcher::Compute m_compute;
};

RoadMatchDispatcher::RoadMatchDispatcher(QObject *parent, Compute compute)
    : QObject(parent), m_thread(RoadWorkerThreads::create(QStringLiteral("road-match"))), m_worker(new RoadMatchWorker(std::move(compute)))
{
    qRegisterMetaType<RoadMatchRequest>();
    qRegisterMetaType<RoadMatchResult>();
    m_clock.start();
    m_worker->moveToThread(m_thread);
    connect(m_worker, &RoadMatchWorker::completed, this, &RoadMatchDispatcher::completed,
            Qt::QueuedConnection);
    // Neither object is owned by the service: a job can finish after the GUI
    // owner is gone. The worker never calls back through a captured GUI pointer.
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
}

RoadMatchDispatcher::~RoadMatchDispatcher()
{
    stop();
}

void RoadMatchDispatcher::stop()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_stopping)
        return;
    m_stopping = true;
    invalidate();
    disconnect(m_worker, nullptr, this, nullptr);
    m_thread->requestInterruption();
    m_thread->quit();
}

void RoadMatchDispatcher::invalidate()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++m_sequence;
    m_pending.reset();
}

void RoadMatchDispatcher::submit(RoadMatchRequest request)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_stopping)
        return;
    request.sequence = ++m_sequence;
    request.submittedMs = m_clock.elapsed();
    if (m_running) {
        if (m_pending)
            ++m_replaced;
        m_pending = std::move(request);
    } else {
        dispatch(std::move(request));
    }
}

void RoadMatchDispatcher::dispatch(RoadMatchRequest request)
{
    m_running = true;
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, request = std::move(request)]() {
        worker->run(request);
    }, Qt::QueuedConnection);
}

void RoadMatchDispatcher::completed(const RoadMatchRequest &request, const RoadMatchResult &result)
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_running = false;
    if (m_stopping)
        return;
    m_lastAgeMs = m_clock.elapsed() - request.submittedMs;
    const bool stale = request.sequence != m_sequence;
    if (stale)
        ++m_stale;
    if ((stale || m_lastAgeMs >= 1000) && m_clock.elapsed() - m_lastLogMs >= 10000) {
        m_lastLogMs = m_clock.elapsed();
        qWarning() << "RoadMatch: sequence" << request.sequence << "latest" << m_sequence
                   << "snapshot-age-ms" << m_lastAgeMs << "stale" << m_stale
                   << "pending-replaced" << m_replaced;
    }
    // Dispatch before publishing, so a reentrant submit from a store signal
    // still sees the correct running state and cannot queue a second job.
    if (m_pending) {
        auto pending = std::move(*m_pending);
        m_pending.reset();
        dispatch(std::move(pending));
    }
    if (!stale)
        emit ready(request, result);
}

#include "RoadMatchDispatcher.moc"

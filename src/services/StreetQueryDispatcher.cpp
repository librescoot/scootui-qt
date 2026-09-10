#include "StreetQueryDispatcher.h"
#include "RoadWorkerThreads.h"
#include <QThread>
#include <QTimer>

class StreetQueryWorker : public QObject
{
    Q_OBJECT
public:
    explicit StreetQueryWorker(StreetQueryDispatcher::Compute compute)
        : m_compute(std::move(compute)) {}
    void run(const StreetQueryRequest &request)
    {
        if (QThread::currentThread()->isInterruptionRequested())
            return;
        const auto result = m_compute(request);
        if (!QThread::currentThread()->isInterruptionRequested())
            emit completed(request.sequence, result);
    }
signals:
    void completed(quint64 sequence, const StreetQueryResult &result);
private:
    StreetQueryDispatcher::Compute m_compute;
};

StreetQueryDispatcher::StreetQueryDispatcher(QObject *parent, Compute compute)
    : QObject(parent), m_thread(RoadWorkerThreads::create(QStringLiteral("roundabout-streets"))),
      m_worker(new StreetQueryWorker(std::move(compute)))
{
    qRegisterMetaType<StreetQueryResult>();
    m_worker->moveToThread(m_thread);
    connect(m_worker, &StreetQueryWorker::completed, this, &StreetQueryDispatcher::completed,
            Qt::QueuedConnection);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start(QThread::LowPriority);
}

StreetQueryDispatcher::~StreetQueryDispatcher() { stop(); }

void StreetQueryDispatcher::invalidate()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++m_sequence;
    ++m_epoch;
    m_pending.reset();
    m_desired.reset();
    m_cachedRequest.reset();
    m_cachedResult = {};
    disconnect(m_ownerDestroyed);
    m_owner.clear();
}

void StreetQueryDispatcher::cancel(QObject *owner)
{
    if (owner == m_owner) {
        disconnect(m_ownerDestroyed);
        m_owner.clear();
        m_desired.reset();
        m_pending.reset();
        ++m_sequence;
    }
}

void StreetQueryDispatcher::stop()
{
    if (m_stopping)
        return;
    m_stopping = true;
    invalidate();
    disconnect(m_worker, nullptr, this, nullptr);
    m_thread->requestInterruption();
    m_thread->quit();
}

bool StreetQueryDispatcher::sameQuery(const StreetQueryRequest &a, const StreetQueryRequest &b)
{
    return a.mapGeneration == b.mapGeneration && a.path == b.path && a.geometryKey == b.geometryKey
        && a.minLat == b.minLat && a.minLon == b.minLon
        && a.maxLat == b.maxLat && a.maxLon == b.maxLon;
}

StreetQueryResult StreetQueryDispatcher::cached(const StreetQueryRequest &request) const
{
    return m_cachedRequest && sameQuery(*m_cachedRequest, request) ? m_cachedResult : StreetQueryResult{};
}

quint64 StreetQueryDispatcher::submit(QObject *owner, StreetQueryRequest request)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_stopping || !owner)
        return 0;
    disconnect(m_ownerDestroyed);
    m_owner = owner;
    m_ownerDestroyed = connect(owner, &QObject::destroyed, this, [this]() { cancel(m_owner); });
    request.sequence = ++m_sequence;
    m_desired = request;
    schedule(std::move(request));
    return m_sequence;
}

void StreetQueryDispatcher::prefetch(StreetQueryRequest request)
{
    Q_ASSERT(QThread::currentThread() == thread());
    // Speculation cannot replace a visible consumer, even while it awaits I/O.
    if (m_stopping || m_owner)
        return;
    request.sequence = ++m_sequence;
    m_desired = request;
    schedule(std::move(request));
}

void StreetQueryDispatcher::schedule(StreetQueryRequest request)
{
    if (cached(request).complete) {
        m_pending.reset();
        if (!m_cacheDeliveryScheduled) {
            m_cacheDeliveryScheduled = true;
            QTimer::singleShot(0, this, &StreetQueryDispatcher::deliverCached);
        }
    } else if (m_running) {
        if (m_activeEpoch == m_epoch && sameQuery(*m_active, request))
            m_pending.reset(); // Promote/reuse an in-flight prefetch, no second read.
        else
            m_pending = std::move(request);
    } else {
        dispatch(std::move(request));
    }
}

void StreetQueryDispatcher::deliverCached()
{
    m_cacheDeliveryScheduled = false;
    if (m_stopping || !m_owner || !m_desired)
        return;
    const auto result = cached(*m_desired);
    if (result.complete)
        emit ready(m_owner, m_desired->sequence, result.streets, true);
}

void StreetQueryDispatcher::dispatch(StreetQueryRequest request)
{
    m_running = true;
    m_active = request;
    m_activeEpoch = m_epoch;
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, request = std::move(request)]() {
        worker->run(request);
    }, Qt::QueuedConnection);
}

void StreetQueryDispatcher::completed(quint64 sequence, const StreetQueryResult &result)
{
    Q_ASSERT(m_active && m_active->sequence == sequence);
    m_running = false;
    if (m_stopping)
        return;
    const bool current = m_activeEpoch == m_epoch && m_desired
        && sameQuery(*m_active, *m_desired);
    if (current && result.complete) {
        m_cachedRequest = *m_active;
        m_cachedResult = result;
    }
    m_active.reset();
    if (m_pending) {
        auto request = std::move(*m_pending);
        m_pending.reset();
        dispatch(std::move(request));
    }
    if (current && m_owner)
        emit ready(m_owner, m_desired->sequence, result.streets, result.complete);
}

#include "StreetQueryDispatcher.moc"

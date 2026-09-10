#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QByteArray>
#include <functional>
#include <optional>

class QThread;
class StreetQueryWorker;

// Worker snapshots contain values only; requester lifetime is tracked on GUI.
struct StreetQueryRequest {
    quint64 sequence = 0;
    int mapGeneration = 0;
    QString path;
    QByteArray geometryKey;
    double minLat = 0, minLon = 0, maxLat = 0, maxLon = 0;
};
struct StreetQueryResult {
    QVariantList streets;
    bool complete = false;
};
StreetQueryResult queryStreets(const StreetQueryRequest &request);

// One running job/result, one replaceable pending snapshot and one cached result.
// A separate worker keeps icon I/O out of the periodic matcher's queue.
class StreetQueryDispatcher : public QObject
{
    Q_OBJECT
public:
    using Compute = std::function<StreetQueryResult(const StreetQueryRequest &)>;
    explicit StreetQueryDispatcher(QObject *parent = nullptr, Compute compute = queryStreets);
    ~StreetQueryDispatcher() override;
    quint64 submit(QObject *owner, StreetQueryRequest request);
    void prefetch(StreetQueryRequest request);
    StreetQueryResult cached(const StreetQueryRequest &request) const;
    void cancel(QObject *owner);
    void invalidate();
    void stop();
    bool running() const { return m_running; }
    bool hasPending() const { return m_pending.has_value(); }

signals:
    void ready(QObject *owner, quint64 sequence, const QVariantList &streets, bool complete);

private:
    void dispatch(StreetQueryRequest request);
    void completed(quint64 sequence, const StreetQueryResult &result);
    void schedule(StreetQueryRequest request);
    void deliverCached();
    static bool sameQuery(const StreetQueryRequest &a, const StreetQueryRequest &b);
    QThread *m_thread;
    StreetQueryWorker *m_worker;
    QPointer<QObject> m_owner;
    QMetaObject::Connection m_ownerDestroyed;
    std::optional<StreetQueryRequest> m_pending;
    std::optional<StreetQueryRequest> m_active;
    std::optional<StreetQueryRequest> m_desired;
    std::optional<StreetQueryRequest> m_cachedRequest;
    StreetQueryResult m_cachedResult;
    quint64 m_epoch = 0;
    quint64 m_activeEpoch = 0;
    bool m_cacheDeliveryScheduled = false;
    quint64 m_sequence = 0;
    bool m_running = false;
    bool m_stopping = false;
};
Q_DECLARE_METATYPE(StreetQueryResult)

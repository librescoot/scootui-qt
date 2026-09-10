#pragma once

#include "RoadMatcher.h"
#include <QObject>
#include <QElapsedTimer>
#include <functional>
#include <optional>

class QThread;
class RoadMatchWorker;

// GUI-thread coordinator: at most one submitted worker event and one replaceable
// pending value. Freshness is relative to cadence-eligible position snapshots,
// not the faster presentation ticks. invalidate() also drops the pending value.
class RoadMatchDispatcher : public QObject
{
    Q_OBJECT
public:
    using Compute = std::function<RoadMatchResult(const RoadMatchRequest &)>;
    explicit RoadMatchDispatcher(QObject *parent = nullptr, Compute compute = matchRoad);
    ~RoadMatchDispatcher() override;
    void submit(RoadMatchRequest request);
    void invalidate();
    void stop();

    quint64 latestSequence() const { return m_sequence; }
    quint64 replacedCount() const { return m_replaced; }
    quint64 staleCount() const { return m_stale; }
    qint64 lastSnapshotAgeMs() const { return m_lastAgeMs; }
    bool running() const { return m_running; }
    bool hasPending() const { return m_pending.has_value(); }

signals:
    void ready(const RoadMatchRequest &request, const RoadMatchResult &result);

private:
    void dispatch(RoadMatchRequest request);
    void completed(const RoadMatchRequest &request, const RoadMatchResult &result);
    QThread *m_thread;
    RoadMatchWorker *m_worker;
    QElapsedTimer m_clock;
    std::optional<RoadMatchRequest> m_pending;
    quint64 m_sequence = 0;
    quint64 m_replaced = 0;
    quint64 m_stale = 0;
    qint64 m_lastAgeMs = 0;
    qint64 m_lastLogMs = -10000;
    bool m_running = false;
    bool m_stopping = false;
};

Q_DECLARE_METATYPE(RoadMatchRequest)
Q_DECLARE_METATYPE(RoadMatchResult)

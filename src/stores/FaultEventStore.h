#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QTimer>

#include "repositories/MdbRepository.h"

// One entry from the events:faults stream, parsed from a QVariantMap.
struct FaultEvent {
    qint64 timestampMs = 0;  // parsed from the "<ms>-<seq>" stream ID
    QString source;           // "group" field (e.g. "engine-ecu", "battery:0")
    int code = 0;             // signed: positive = raise, negative = clear
    QString description;      // empty on clear or when missing
};

// Tails the events:faults Redis stream on a polling timer. Polls at a
// slow cadence by default so the active-count badge stays fresh; callers
// can bump the interval up (setPollIntervalMs) while a screen is reading
// the events.
class FaultEventStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY eventsChanged)

public:
    explicit FaultEventStore(MdbRepository *repo, QObject *parent = nullptr);

    int count() const { return m_events.size(); }
    const QVector<FaultEvent> &events() const { return m_events; }

    // Number of entries requested per XREVRANGE.
    static constexpr int kFetchCount = 500;

    Q_INVOKABLE void refresh();

    void setPollIntervalMs(int intervalMs);
    int pollIntervalMs() const { return m_pollIntervalMs; }

    void start();
    void stop();

signals:
    void eventsChanged();

private slots:
    void onStreamFetched(const QString &key, const QVariantList &entries);

private:
    MdbRepository *m_repo;
    QTimer *m_timer;
    QVector<FaultEvent> m_events;
    int m_pollIntervalMs = 30000;
};

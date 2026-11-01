#include "TripHistoryService.h"
#include "RoutePlanRpc.h"

#include <QJsonArray>
#include <QJsonObject>

TripHistoryService::TripHistoryService(MdbRepository *repo, QObject *parent)
    : QObject(parent), m_rpc(new RoutePlanRpc(repo, QStringLiteral("trip:history:rpc"), this))
{
}

void TripHistoryService::refresh()
{
    ++m_generation;
    m_rpc->cancelAll();
    m_trips.clear();
    m_selected.clear();
    m_points.clear();
    m_cursor.clear();
    m_error.clear();
    m_loading = true;
    emit changed();
    fetchList({}, m_generation);
}

void TripHistoryService::loadMore()
{
    if (m_loading || m_cursor.isEmpty()) return;
    m_loading = true;
    m_error.clear();
    emit changed();
    fetchList(m_cursor, m_generation);
}

void TripHistoryService::fetchList(const QString &cursor, quint64 generation)
{
    m_rpc->call(QStringLiteral("history.list"), {{QStringLiteral("cursor"), cursor}, {QStringLiteral("limit"), 20}},
                [this, generation](const QJsonObject &result, const QString &error) {
        if (generation != m_generation) return;
        m_loading = false;
        m_error = error;
        if (error.isEmpty()) {
            for (const auto &trip : result.value(QStringLiteral("trips")).toArray())
                m_trips.append(trip.toObject().toVariantMap());
            m_cursor = result.value(QStringLiteral("next_cursor")).toString();
        }
        emit changed();
    });
}

void TripHistoryService::selectTrip(const QString &id)
{
    if (m_loading || id.isEmpty()) return;
    const quint64 generation = ++m_generation;
    m_selected.clear();
    m_points.clear();
    m_error.clear();
    m_loading = true;
    emit changed();
    m_rpc->call(QStringLiteral("history.get"), {{QStringLiteral("id"), id}},
                [this, generation](const QJsonObject &result, const QString &error) {
        if (generation != m_generation) return;
        m_loading = false;
        m_error = error;
        if (error.isEmpty()) m_selected = result.toVariantMap();
        emit changed();
    });
}

void TripHistoryService::loadPoints()
{
    const QString id = m_selected.value(QStringLiteral("id")).toString();
    if (m_loading || id.isEmpty()) return;
    const quint64 generation = ++m_generation;
    m_points.clear();
    m_error.clear();
    m_loading = true;
    emit changed();
    fetchPoints(0, generation, id);
}

void TripHistoryService::fetchPoints(qint64 after, quint64 generation, const QString &tripId)
{
    m_rpc->call(QStringLiteral("history.points"), {{QStringLiteral("id"), tripId},
                {QStringLiteral("after_id"), double(after)}, {QStringLiteral("limit"), 100}},
                [this, generation, tripId, after](const QJsonObject &result, const QString &error) {
        if (generation != m_generation) return;
        if (!error.isEmpty()) {
            m_loading = false;
            m_error = error;
            emit changed();
            return;
        }
        for (const auto &point : result.value(QStringLiteral("points")).toArray())
            m_points.append(point.toObject().toVariantMap());
        const qint64 next = qint64(result.value(QStringLiteral("next_after_id")).toDouble());
        if (next > 0 && next <= after) {
            m_loading = false;
            m_error = QStringLiteral("invalid_cursor");
            emit changed();
            return;
        }
        if (next > 0 && m_points.size() < 10000) {
            fetchPoints(next, generation, tripId);
            return;
        }
        m_loading = false;
        if (next > 0) m_error = QStringLiteral("route_too_long");
        emit changed();
    });
}

void TripHistoryService::close()
{
    ++m_generation;
    m_rpc->cancelAll();
    if (m_loading) {
        m_loading = false;
        emit changed();
    }
}

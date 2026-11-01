#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class MdbRepository;
class RoutePlanRpc;

class TripHistoryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList trips READ trips NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QVariantList points READ points NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit TripHistoryService(MdbRepository *repo, QObject *parent = nullptr);
    QVariantList trips() const { return m_trips; }
    QVariantMap selected() const { return m_selected; }
    QVariantList points() const { return m_points; }
    bool loading() const { return m_loading; }
    bool hasMore() const { return !m_cursor.isEmpty(); }
    QString error() const { return m_error; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void selectTrip(const QString &id);
    Q_INVOKABLE void loadPoints();
    Q_INVOKABLE void close();
signals:
    void changed();
private:
    void fetchList(const QString &cursor, quint64 generation);
    void fetchPoints(qint64 after, quint64 generation, const QString &tripId);
    RoutePlanRpc *m_rpc;
    QVariantList m_trips;
    QVariantMap m_selected;
    QVariantList m_points;
    QString m_cursor;
    QString m_error;
    bool m_loading = false;
    quint64 m_generation = 0;
};

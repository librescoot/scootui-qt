#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include "RouteModels.h"

class ValhallaClient : public QObject
{
    Q_OBJECT

public:
    explicit ValhallaClient(QObject *parent = nullptr);

    void setEndpoint(const QString &url);
    void setLanguage(const QString &lang);

    // Coalesces rapid calls: the pending request is overwritten until the
    // debounce timer fires, at which point the latest (from, to) is sent and
    // any earlier in-flight reply is aborted.
    void calculateRoute(const LatLng &from, const LatLng &to);

    void checkStatus();

signals:
    void routeCalculated(const Route &route);
    void routeError(const QString &error);
    void rateLimited();
    void statusChecked(bool available);

private:
    void dispatchPending();
    void handleRouteReply(QNetworkReply *reply);
    Route parseRouteResponse(const QByteArray &data);

    static constexpr int DebounceIntervalMs = 200;

    QNetworkAccessManager m_nam;
    QString m_endpoint;
    QString m_language = QStringLiteral("en-US");

    // Debounce: the latest pending request, dispatched when m_debounce fires
    QTimer m_debounce;
    LatLng m_pendingFrom;
    LatLng m_pendingTo;
    bool m_hasPending = false;

    // Tracks the active reply so we can abort on dispatch of a new request
    QPointer<QNetworkReply> m_activeReply;
};

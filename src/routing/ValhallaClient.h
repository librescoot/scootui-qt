#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include "RouteModels.h"

class ValhallaClient : public QObject
{
    Q_OBJECT

public:
    explicit ValhallaClient(QObject *parent = nullptr);

    void setEndpoint(const QString &url);
    void calculateRoute(const LatLng &from, const LatLng &to);
    void checkStatus();

signals:
    void routeCalculated(const Route &route);
    void routeError(const QString &error);
    void rateLimited();
    void statusChecked(bool available);

private:
    void handleRouteReply(QNetworkReply *reply);
    Route parseRouteResponse(const QByteArray &data);

    QNetworkAccessManager m_nam;
    QString m_endpoint;
};

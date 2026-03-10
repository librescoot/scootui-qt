#pragma once

#include <QObject>
#include <QNetworkAccessManager>

class ReverseGeocodingService : public QObject
{
    Q_OBJECT

public:
    explicit ReverseGeocodingService(QObject *parent = nullptr);

    Q_INVOKABLE void resolve(double latitude, double longitude);

signals:
    void addressResolved(const QString &address);
    void resolveFailed(const QString &error);

private:
    QNetworkAccessManager *m_nam;
};

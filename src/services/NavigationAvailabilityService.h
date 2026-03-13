#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>

class SettingsStore;
class InternetStore;
class MdbRepository;

class NavigationAvailabilityService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool localDisplayMapsAvailable READ localDisplayMapsAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(bool routingAvailable READ routingAvailable NOTIFY availabilityChanged)

public:
    explicit NavigationAvailabilityService(SettingsStore *settings, InternetStore *internet,
                                            MdbRepository *repo, QObject *parent = nullptr);

    bool localDisplayMapsAvailable() const { return m_mapsAvailable; }
    bool routingAvailable() const { return m_routingAvailable; }

    Q_INVOKABLE void recheck();

signals:
    void availabilityChanged();

private:
    void checkMaps();
    void checkRouting();
    void publishToRedis();
    void scheduleRetry();

    SettingsStore *m_settings;
    InternetStore *m_internet;
    MdbRepository *m_repo;
    QNetworkAccessManager *m_nam;
    QTimer m_retryTimer;
    bool m_mapsAvailable = false;
    bool m_routingAvailable = false;
};

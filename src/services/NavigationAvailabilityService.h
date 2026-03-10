#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>

class SettingsStore;
class InternetStore;

class NavigationAvailabilityService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool localDisplayMapsAvailable READ localDisplayMapsAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(bool routingAvailable READ routingAvailable NOTIFY availabilityChanged)

public:
    explicit NavigationAvailabilityService(SettingsStore *settings, InternetStore *internet,
                                            QObject *parent = nullptr);

    bool localDisplayMapsAvailable() const { return m_mapsAvailable; }
    bool routingAvailable() const { return m_routingAvailable; }

    Q_INVOKABLE void recheck();

signals:
    void availabilityChanged();

private:
    void checkMaps();
    void checkRouting();

    SettingsStore *m_settings;
    InternetStore *m_internet;
    QNetworkAccessManager *m_nam;
    bool m_mapsAvailable = false;
    bool m_routingAvailable = false;
};

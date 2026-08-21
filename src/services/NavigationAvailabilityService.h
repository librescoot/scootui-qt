#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QtQml/qqmlengine.h>

class SettingsStore;
class InternetStore;
class MdbRepository;

class NavigationAvailabilityService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool localDisplayMapsAvailable READ localDisplayMapsAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(bool routingAvailable READ routingAvailable NOTIFY availabilityChanged)

public:
    explicit NavigationAvailabilityService(SettingsStore *settings, InternetStore *internet,
                                            MdbRepository *repo, QObject *parent = nullptr);

    bool localDisplayMapsAvailable() const { return m_mapsAvailable; }
    bool routingAvailable() const { return m_routingAvailable; }

    Q_INVOKABLE void recheck();
    Q_INVOKABLE void setOverride(bool maps, bool routing);
    Q_INVOKABLE void clearOverride();

signals:
    void availabilityChanged();
    // Edge-triggered: fires only on the false->true transition of the local
    // display maps flag (i.e. /data/maps/map.mbtiles just became reachable,
    // typically after a late /data mount the poller finally saw). Consumers
    // reload their mbtiles handles off this instead of the level-triggered
    // availabilityChanged(), which also fires on routing-only flaps.
    void localMapsBecameAvailable();

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
    int m_retryDelayMs = 1000;
    QTimer m_mapsRetryTimer;
    int m_mapsRetryDelayMs = 1000;
    bool m_mapsAvailable = false;
    bool m_routingAvailable = false;
    bool m_overrideActive = false;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static NavigationAvailabilityService *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(NavigationAvailabilityService *instance) { s_qmlInstance = instance; }

private:
    static inline NavigationAvailabilityService *s_qmlInstance = nullptr;
};

#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

#include "AttentionPolicy.h"

class VehicleStore;

class NotificationService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap presentation READ presentation NOTIFY presentationChanged)
    Q_PROPERTY(QVariantList active READ active NOTIFY activeChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool mapUpdateAvailable READ mapUpdateAvailable NOTIFY mapUpdateAvailableChanged)
    Q_PROPERTY(bool coverageWarning READ coverageWarning NOTIFY coverageWarningChanged)
    Q_PROPERTY(bool telemetryTrustLost READ telemetryTrustLost NOTIFY telemetryTrustChanged)
    Q_PROPERTY(bool simulatorInjectionEnabled READ simulatorInjectionEnabled CONSTANT)
    Q_PROPERTY(QString surface READ surface WRITE setSurface NOTIFY surfaceChanged)

public:
    // An injected clock must be monotonic; production uses the elapsed timer.
    explicit NotificationService(bool simulatorInjectionEnabled = false,
                                 QObject *parent = nullptr,
                                 std::function<qint64()> clock = {});

    QVariantMap presentation() const { return m_presentation; }
    QVariantList active() const;
    QVariantList history() const;
    bool mapUpdateAvailable() const { return m_mapUpdateAvailable; }
    bool coverageWarning() const { return m_coverageWarning; }
    bool telemetryTrustLost() const { return !m_telemetryConnected || m_conditions.contains(QStringLiteral("redis-disconnect")); }
    void setTelemetryConnected(bool connected);
    bool simulatorInjectionEnabled() const { return m_simulatorInjectionEnabled; }
    QString surface() const { return m_surface; }
    void setSurface(const QString &surface);

    void setNavigationPayload(const QVariantMap &navigation);
    void setVehicleStore(VehicleStore *vehicle);

    Q_INVOKABLE QString publishCondition(const QString &id, const QString &source,
                                         const QString &title, const QString &body,
                                         int priority = 2, const QString &kind = QStringLiteral("warning"),
                                         const QString &icon = {});
    Q_INVOKABLE void resolveCondition(const QString &id);
    Q_INVOKABLE QString publishEvent(const QString &id, const QString &source,
                                     const QString &title, const QString &body,
                                     int priority = 4, const QString &kind = QStringLiteral("info"),
                                     int lifetimeMs = 4000);
    Q_INVOKABLE void clearEvent(const QString &id);

    void setMapUpdateAvailable(bool available, const QString &title = {});
    void setCoverageWarning(bool warning);

    // These methods only mutate the local notification registry. They are
    // exposed only when the simulator panel is explicitly enabled.
    Q_INVOKABLE void simulateWarning(const QString &title, const QString &body);
    Q_INVOKABLE void simulateError(const QString &title, const QString &body);
    Q_INVOKABLE void simulateCritical(const QString &title, const QString &body);
    Q_INVOKABLE void clearSimulatorEntries();

signals:
    void presentationChanged();
    void activeChanged();
    void historyChanged();
    void mapUpdateAvailableChanged();
    void coverageWarningChanged();
    void surfaceChanged();
    void telemetryTrustChanged();
    void eventPresented(const QString &kind);
    void conditionPresented(const QString &kind);

private:
    qint64 nowMs() const;
    void refreshPresentation();
    void emitPresentationCue(const QVariantMap &main);
    void onVehicleStateChanged();
    Q_SLOT void expireEvents();
    QVariantMap makeEntry(const QString &id, const QString &source, const QString &title,
                          const QString &body, int priority, const QString &kind) const;

    QHash<QString, QVariantMap> m_conditions;
    QList<QVariantMap> m_events;
    QList<QVariantMap> m_history;
    QHash<QString, int> m_conditionCuePriorities;
    QHash<QString, int> m_eventCuePriorities;
    QVariantMap m_navigation;
    QVariantMap m_presentation;
    VehicleStore *m_vehicleStore = nullptr;
    QTimer m_expiryTimer;
    QElapsedTimer m_clock;
    std::function<qint64()> m_nowMs;
    AttentionCycleState m_cycle;
    QString m_surface = QStringLiteral("cluster");
    bool m_riding = false;
    bool m_telemetryConnected = true;
    bool m_mapUpdateAvailable = false;
    bool m_coverageWarning = false;
    bool m_simulatorInjectionEnabled = false;
    quint64 m_nextOrder = 1;
};

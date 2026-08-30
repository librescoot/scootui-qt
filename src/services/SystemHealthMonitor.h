#pragma once

#include <QObject>
#include <QTimer>

class VehicleStore;
class ConnectionStore;
class ToastService;
class Translations;

// Decides when the dashboard falls back to the maintenance screen and raises
// the permanent connection-loss toasts. Moved out of Main.qml so the health
// policy has one owner: an unexpected vehicle state, a prolonged Redis
// disconnect before ever connecting, or a state that never leaves Unknown
// past the startup grace period all route through here.
class SystemHealthMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showMaintenance READ showMaintenance NOTIFY healthChanged)
    // Connection details only for genuine connection failures, not for
    // locked/transitional vehicle states.
    Q_PROPERTY(bool showConnectionInfo READ showConnectionInfo NOTIFY healthChanged)

public:
    explicit SystemHealthMonitor(VehicleStore *vehicle, ConnectionStore *connection,
                                 ToastService *toasts, Translations *translations,
                                 QObject *parent = nullptr);

    bool showMaintenance() const;
    bool showConnectionInfo() const;

signals:
    void healthChanged();

private:
    void onVehicleStateChanged();
    void onProlongedDisconnectChanged();
    void onUsingBackupConnectionChanged();
    bool neverConnected() const;
    bool unknownPastGrace() const;

    VehicleStore *m_vehicle;
    ConnectionStore *m_connection;
    ToastService *m_toasts;
    Translations *m_translations;

    // A dashboard that boots faster than vehicle-service publishes its state
    // would otherwise flash the maintenance screen on every start.
    QTimer m_startupGrace;
    bool m_startupGraceElapsed = false;
    static constexpr int kStartupGraceMs = 5000;
};

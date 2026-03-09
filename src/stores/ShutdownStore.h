#pragma once

#include <QObject>

class VehicleStore;

class ShutdownStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isShuttingDown READ isShuttingDown NOTIFY shuttingDownChanged)
    Q_PROPERTY(bool showBlackout READ showBlackout NOTIFY showBlackoutChanged)

public:
    explicit ShutdownStore(QObject *parent = nullptr);

    bool isShuttingDown() const { return m_isShuttingDown; }
    bool showBlackout() const { return m_showBlackout; }

    void connectToVehicle(VehicleStore *vehicle);

    Q_INVOKABLE void beginShutdown();
    Q_INVOKABLE void animationComplete();
    void forceBlackout();

signals:
    void shuttingDownChanged();
    void showBlackoutChanged();
    void requestPoweroff();

private:
    void onVehicleStateChanged();

    VehicleStore *m_vehicle = nullptr;
    bool m_isShuttingDown = false;
    bool m_showBlackout = false;
    bool m_poweroffScheduled = false;
    bool m_wasInDriveState = false;
};

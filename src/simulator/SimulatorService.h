#pragma once

#include <QObject>
#include <QTimer>
#include "routing/RouteModels.h"

class MdbRepository;
class NavigationService;

class SimulatorService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autoDriveActive READ autoDriveActive NOTIFY autoDriveActiveChanged)
    Q_PROPERTY(double autoDriveSpeed READ autoDriveSpeed NOTIFY autoDriveSpeedChanged)
    Q_PROPERTY(bool simulatorMode READ simulatorMode CONSTANT)

public:
    explicit SimulatorService(MdbRepository *repo, NavigationService *nav, QObject *parent = nullptr);

    bool autoDriveActive() const { return m_autoDriveActive; }
    double autoDriveSpeed() const { return m_autoDriveSpeed; }
    bool simulatorMode() const { return true; }

    // Vehicle
    Q_INVOKABLE void setVehicleState(const QString &state);
    Q_INVOKABLE void setKickstand(const QString &state);
    Q_INVOKABLE void setBlinkerState(const QString &state);
    Q_INVOKABLE void setBrakeLeft(bool pressed);
    Q_INVOKABLE void setBrakeRight(bool pressed);
    Q_INVOKABLE void setSeatboxLock(const QString &state);
    Q_INVOKABLE void setSeatboxButton(bool pressed);
    Q_INVOKABLE void setHandlebarLock(const QString &state);
    Q_INVOKABLE void setHornButton(bool pressed);

    // Gestures
    Q_INVOKABLE void simulateBrakeTap(const QString &side);
    Q_INVOKABLE void simulateBrakeHold(const QString &side, int durationMs = 3000);
    Q_INVOKABLE void simulateBrakeDoubleTap(const QString &side);
    Q_INVOKABLE void simulateSeatboxTap();
    Q_INVOKABLE void simulateSeatboxHold(int durationMs = 2000);
    Q_INVOKABLE void simulateSeatboxDoubleTap();

    // Engine
    Q_INVOKABLE void setSpeed(double speed);
    Q_INVOKABLE void setOdometer(double km);
    Q_INVOKABLE void setEngineTemperature(double temp);
    Q_INVOKABLE void setMotorPower(bool on);
    Q_INVOKABLE void setThrottle(bool on);

    // Battery (slot 0 or 1)
    Q_INVOKABLE void setBatteryCharge(int slot, int percent);
    Q_INVOKABLE void setBatteryPresent(int slot, bool present);
    Q_INVOKABLE void setBatteryState(int slot, const QString &state);
    Q_INVOKABLE void setBatteryVoltage(int slot, int millivolts);
    Q_INVOKABLE void setBatteryCurrent(int slot, int milliamps);
    Q_INVOKABLE void setBatteryTemperature(int slot, int temp);

    // GPS
    Q_INVOKABLE void setGpsPosition(double lat, double lng);
    Q_INVOKABLE void setGpsCourse(double course);
    Q_INVOKABLE void setGpsSpeed(double speed);
    Q_INVOKABLE void setGpsState(const QString &state);

    // Internet
    Q_INVOKABLE void setModemState(const QString &state);
    Q_INVOKABLE void setSignalQuality(int quality);
    Q_INVOKABLE void setAccessTech(const QString &tech);
    Q_INVOKABLE void setCloudConnection(const QString &state);

    // Bluetooth
    Q_INVOKABLE void setBluetoothStatus(const QString &state);

    // Speed limit
    Q_INVOKABLE void setSpeedLimit(const QString &limit);
    Q_INVOKABLE void setRoadName(const QString &name);
    Q_INVOKABLE void setRoadType(const QString &type);

    // Settings
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE void setLanguage(const QString &lang);
    Q_INVOKABLE void setDualBattery(bool enabled);

    // Presets
    Q_INVOKABLE void loadPreset(const QString &name);

    // Routes
    Q_INVOKABLE void loadTestRoute(int index);

    // Auto-drive
    Q_INVOKABLE void startAutoDrive(double targetSpeed);
    Q_INVOKABLE void stopAutoDrive();

signals:
    void autoDriveActiveChanged();
    void autoDriveSpeedChanged();

private:
    void autoDriveTick();
    void updateRoadInfo();
    void applyDefaults();
    void setBatteryField(int slot, const QString &field, const QString &value);

    int m_currentInstructionIndex = 0;

    MdbRepository *m_repo;
    NavigationService *m_nav;
    QTimer *m_autoDriveTimer = nullptr;
    bool m_autoDriveActive = false;
    double m_autoDriveSpeed = 0;
    double m_autoDriveTargetSpeed = 25;
    double m_autoDriveLat = 52.520008;
    double m_autoDriveLng = 13.404954;
    double m_autoDriveBearing = 0;
    Route m_route;
    int m_routeWaypointIndex = 0;
    double m_batteryCharge0 = 80;
    double m_batteryCharge1 = 80;
    double m_odometer = 1234.5;
};

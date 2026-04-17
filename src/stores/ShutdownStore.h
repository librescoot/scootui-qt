#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class VehicleStore;

class QQmlEngine;
class QJSEngine;

class ShutdownStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool isShuttingDown READ isShuttingDown NOTIFY shuttingDownChanged)
    Q_PROPERTY(bool showBlackout READ showBlackout NOTIFY showBlackoutChanged)

public:
    explicit ShutdownStore(QObject *parent = nullptr);
    static ShutdownStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    bool isShuttingDown() const { return m_isShuttingDown; }
    bool showBlackout() const { return m_showBlackout; }

    void connectToVehicle(VehicleStore *vehicle);

    Q_INVOKABLE void beginShutdown();
    void forceBlackout();
    void resetShutdown();

signals:
    void shuttingDownChanged();
    void showBlackoutChanged();

private:
    void onVehicleStateChanged();

    VehicleStore *m_vehicle = nullptr;
    bool m_isShuttingDown = false;
    bool m_showBlackout = false;
    bool m_wasInDriveState = false;

    static inline ShutdownStore *s_instance = nullptr;
};

#pragma once

#include <QObject>
#include <QtQml/qqmlengine.h>

class VehicleStore;

class ShutdownStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool isShuttingDown READ isShuttingDown NOTIFY shuttingDownChanged)
    Q_PROPERTY(bool showBlackout READ showBlackout NOTIFY showBlackoutChanged)

public:
    // parent is deliberately not defaulted: a default-constructible type makes Qt
    // pick SingletonConstructionMode::Constructor and build its own instance
    // instead of calling create(), which would hand QML an unwired object.
    explicit ShutdownStore(QObject *parent);

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

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static ShutdownStore *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(ShutdownStore *instance) { s_qmlInstance = instance; }

private:
    static inline ShutdownStore *s_qmlInstance = nullptr;
};

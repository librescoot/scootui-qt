#pragma once

#include <QObject>
#include <QString>

class MdbRepository;

// The one place scootui issues commands to the rest of the vehicle. Stores
// mirror Redis state and stay read-only; whatever changes vehicle state goes
// through a named method here, so the entire outbound surface is greppable
// in one file.
class CommandBus : public QObject
{
    Q_OBJECT

public:
    explicit CommandBus(MdbRepository *repo, QObject *parent = nullptr);

    // vehicle-service
    void lockVehicle();
    // Both -> off, anything else -> both. Callers pass the blinker state they
    // see so the toggle decision lives here exactly once.
    void toggleHazards(int currentBlinkerState);
    void hopOnEngage();
    void hopOnEngageLearning();
    void hopOnRelease();
    // DBC power hold; vehicle-service caps the duration, so a dashboard that
    // dies mid hold cannot pin power on.
    void holdDbc(const QString &reason);
    void releaseDbcHold();

    // bluetooth-service
    void deleteAllBluetoothBonds();

    // settings-service service-mode overlay
    void applyServiceOverlay();
    void clearServiceOverlay();

    // dashboard hash flags observed by vehicle-service
    void setMenuOpen(bool open);
    void setDebugMode(const QString &mode);
    // dbc-backlight-service turns the panel off/on; hop-on lock and the
    // maintenance screen use it.
    Q_INVOKABLE void setBacklightEnabled(bool enabled);

    // vehicle-service / ums-service: expose /data as USB mass storage
    void enterUmsMode();

private:
    MdbRepository *m_repo;
};

#pragma once

#include <QObject>

class VehicleStore;
class MdbRepository;

// Subscribes to the "input-events" pub/sub channel published by
// vehicle-service (see vehicle-service/internal/core/input_gestures.go)
// and re-emits high-level brake gesture signals. Consumers connect to
// the signals and apply their own context guards.
class InputHandler : public QObject
{
    Q_OBJECT

public:
    explicit InputHandler(VehicleStore *vehicle, MdbRepository *repo,
                          QObject *parent = nullptr);
    ~InputHandler() override;

signals:
    void leftTap();         // "brake:left:tap"
    void leftHold();        // "brake:left:long-tap" (fires while still held)
    void leftDoubleTap();   // "brake:left:double-tap"
    void rightTap();        // "brake:right:tap"
    void rightHold();       // "brake:right:long-tap" (fires while still held)

    // "brake:<side>:hold" — the 3s hold, distinct from the 800ms long-tap
    // behind leftHold()/rightHold(). The left one is the gesture ums-service
    // exits UMS on, so anything mirroring that decision must key off this
    // signal to stay in step with the MDB. Bind either only to close or
    // cancel, so the long hold means one thing wherever it is wired.
    void leftBrakeHold();
    void rightBrakeHold();

private:
    void onInputEvent(const QString &message);

    VehicleStore *m_vehicle;
    MdbRepository *m_repo;
};

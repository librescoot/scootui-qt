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

private:
    void onInputEvent(const QString &message);

    VehicleStore *m_vehicle;
    MdbRepository *m_repo;
};

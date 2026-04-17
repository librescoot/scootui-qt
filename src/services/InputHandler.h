#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class VehicleStore;
class MdbRepository;

// Subscribes to the "input-events" pub/sub channel published by
// vehicle-service (see vehicle-service/internal/core/input_gestures.go)
// and re-emits high-level brake gesture signals. Consumers connect to
// the signals and apply their own context guards.
class QQmlEngine;
class QJSEngine;

class InputHandler : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit InputHandler(VehicleStore *vehicle, MdbRepository *repo,
                          QObject *parent = nullptr);
    static InputHandler *create(QQmlEngine *, QJSEngine *) { return s_instance; }
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

    static inline InputHandler *s_instance = nullptr;
};

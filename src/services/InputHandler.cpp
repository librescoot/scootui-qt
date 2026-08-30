#include "InputHandler.h"
#include "stores/VehicleStore.h"
#include "repositories/MdbRepository.h"

#include <QDebug>
#include "../repositories/RedisSchema.h"

InputHandler::InputHandler(VehicleStore *vehicle, MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
    , m_repo(repo)
{
    if (m_repo) {
        m_repo->subscribe(RedisSchema::channel::InputEvents,
                          [this](const QString &, const QString &msg) {
            onInputEvent(msg);
        });
    }
}

InputHandler::~InputHandler()
{
    if (m_repo)
        m_repo->unsubscribe(RedisSchema::channel::InputEvents);
}

void InputHandler::onInputEvent(const QString &message)
{
    // Format from vehicle-service: "<source>:<gesture>" or
    // "<source>:<side>:<gesture>".
    QStringList parts = message.split(':');

    if (parts.size() == 2 && parts[0] == QLatin1String("seatbox")) {
        const QString &gesture = parts[1];
        if (gesture == QLatin1String("press"))
            emit seatboxPress();
        else if (gesture == QLatin1String("long-tap"))
            emit seatboxLongTap();
        else if (gesture == QLatin1String("release"))
            emit seatboxRelease();
        else if (gesture == QLatin1String("double-tap"))
            emit seatboxDoubleTap();
        return;
    }

    if (parts.size() < 3 || parts[0] != QLatin1String("brake"))
        return;

    const QString &side = parts[1];
    const QString &gesture = parts[2];
    const bool isDoubleTap = (gesture == QLatin1String("double-tap"));

    if (isDoubleTap)
        qDebug() << "InputHandler: received" << message << "vehicleState" << m_vehicle->state();

    if (!m_vehicle->isParked()) {
        if (isDoubleTap)
            qDebug() << "InputHandler: dropped" << message << "- not parked, vehicleState" << m_vehicle->state();
        return;
    }

    if (side == QLatin1String("left")) {
        if (gesture == QLatin1String("tap"))
            emit leftTap();
        else if (gesture == QLatin1String("long-tap"))
            emit leftHold();
        else if (gesture == QLatin1String("hold"))
            emit leftBrakeHold();
        else if (gesture == QLatin1String("double-tap")) {
            qDebug() << "InputHandler: emitting leftDoubleTap";
            emit leftDoubleTap();
        }
    } else if (side == QLatin1String("right")) {
        if (gesture == QLatin1String("tap"))
            emit rightTap();
        else if (gesture == QLatin1String("long-tap"))
            emit rightHold();
        else if (gesture == QLatin1String("hold"))
            emit rightBrakeHold();
    }
}

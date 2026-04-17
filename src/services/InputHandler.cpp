#include "InputHandler.h"
#include "stores/VehicleStore.h"
#include "repositories/MdbRepository.h"

namespace {
constexpr char kInputEventsChannel[] = "input-events";
}

InputHandler::InputHandler(VehicleStore *vehicle, MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
    , m_repo(repo)
{
    s_instance = this;
    if (m_repo) {
        m_repo->subscribe(QLatin1String(kInputEventsChannel),
                          [this](const QString &, const QString &msg) {
            onInputEvent(msg);
        });
    }
}

InputHandler::~InputHandler()
{
    if (m_repo)
        m_repo->unsubscribe(QLatin1String(kInputEventsChannel));
}

void InputHandler::onInputEvent(const QString &message)
{
    // Format from vehicle-service: "<source>:<gesture>" or
    // "<source>:<side>:<gesture>". We only care about brakes here;
    // seatbox/horn gestures are consumed by other stores.
    QStringList parts = message.split(':');
    if (parts.size() < 3 || parts[0] != QLatin1String("brake"))
        return;

    if (!m_vehicle->isParked())
        return;

    const QString &side = parts[1];
    const QString &gesture = parts[2];

    if (side == QLatin1String("left")) {
        if (gesture == QLatin1String("tap"))
            emit leftTap();
        else if (gesture == QLatin1String("long-tap"))
            emit leftHold();
        else if (gesture == QLatin1String("double-tap"))
            emit leftDoubleTap();
    } else if (side == QLatin1String("right")) {
        if (gesture == QLatin1String("tap"))
            emit rightTap();
    }
}

#pragma once

#include "models/Enums.h"

#include <QHash>
#include <QObject>
#include <QString>

class BatteryStore;
class QSoundEffect;
class ToastService;
class VehicleStore;

enum class SoundEvent {
    None,
    VehicleWake,
    VehicleReady,
    VehicleParked,
    VehicleShutdown,
    BlinkerPulse,
    BlinkerOff,
    BatteryInserted,
    BatteryRemoved,
    SeatboxOpened,
    SeatboxClosed,
    NotificationInfo,
    NotificationSuccess,
    NotificationWarning,
    NotificationError,
};

enum class SoundCue {
    None,
    Wake,
    Ready,
    Parked,
    Shutdown,
    BlinkerPulse,
    BlinkerOff,
    BatteryInsert,
    BatteryRemove,
    SeatboxOpen,
    SeatboxClosed,
    Info,
    Success,
    Warning,
    Error,
};

namespace SoundCueMapping {
SoundEvent vehicleTransition(ScootEnums::VehicleState from, ScootEnums::VehicleState to);
SoundEvent blinkerPhase(qreal previousOpacity, qreal opacity, bool active);
SoundEvent blinkerTransition(ScootEnums::BlinkerState from, ScootEnums::BlinkerState to);
SoundEvent batteryPresence(bool wasPresent, bool present);
SoundEvent seatboxTransition(ScootEnums::SeatboxLock from, ScootEnums::SeatboxLock to);
SoundEvent notification(const QString &type);
SoundCue cueForEvent(SoundEvent event);
}

class SoundCueService : public QObject
{
    Q_OBJECT

public:
    static constexpr qreal DefaultVolume = 0.80;

    explicit SoundCueService(VehicleStore *vehicleStore, BatteryStore *battery0Store,
                             BatteryStore *battery1Store, ToastService *toastService,
                             const QString &assetRoot = QStringLiteral("qrc:/ScootUI/assets/sounds"),
                             QObject *parent = nullptr);

    static bool validateWaveFile(const QString &path, QString *error = nullptr);
    void arm();

private:
    void loadCues(const QString &assetRoot);
    void disableAudio(const QString &reason);
    void playEvent(SoundEvent event);
    void playCue(SoundCue cue);

    VehicleStore *m_vehicleStore;
    BatteryStore *m_battery0Store;
    BatteryStore *m_battery1Store;
    QHash<SoundCue, QSoundEffect *> m_effects;
    ScootEnums::VehicleState m_vehicleState;
    ScootEnums::SeatboxLock m_seatboxState;
    ScootEnums::BlinkerState m_blinkerState;
    qreal m_blinkOpacity = 0.0;
    bool m_battery0Present = false;
    bool m_battery1Present = false;
    bool m_armed = false;
    bool m_audioAvailable = false;
};

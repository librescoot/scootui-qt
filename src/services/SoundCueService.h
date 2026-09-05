#pragma once

#include "models/Enums.h"

#include <QHash>
#include <QObject>
#include <QString>

class QSoundEffect;
class ToastService;
class VehicleStore;

enum class SoundEvent {
    None,
    VehicleReady,
    VehicleParked,
    VehicleShutdown,
    IndicatorOn,
    IndicatorOff,
    NotificationInfo,
    NotificationSuccess,
    NotificationWarning,
    NotificationError,
};

enum class SoundCue {
    None,
    Ready,
    Parked,
    Shutdown,
    IndicatorOn,
    IndicatorOff,
    Info,
    Success,
    Warning,
    Error,
};

namespace SoundCueMapping {
SoundEvent vehicleTransition(ScootEnums::VehicleState from, ScootEnums::VehicleState to);
SoundEvent indicatorTransition(ScootEnums::BlinkerState from, ScootEnums::BlinkerState to);
SoundEvent notification(const QString &type);
SoundCue cueForEvent(SoundEvent event);
}

class SoundCueService : public QObject
{
    Q_OBJECT

public:
    static constexpr qreal DefaultVolume = 0.80;

    explicit SoundCueService(VehicleStore *vehicleStore, ToastService *toastService,
                             const QString &assetRoot = QStringLiteral("qrc:/ScootUI/assets/sounds"),
                             QObject *parent = nullptr);

    static bool validateWaveFile(const QString &path, QString *error = nullptr);

private:
    void loadCues(const QString &assetRoot);
    void playEvent(SoundEvent event);
    void playCue(SoundCue cue);

    VehicleStore *m_vehicleStore;
    QHash<SoundCue, QSoundEffect *> m_effects;
    ScootEnums::VehicleState m_vehicleState;
    ScootEnums::BlinkerState m_blinkerState;
};

#include "SoundCueService.h"

#include "services/ToastService.h"
#include "stores/VehicleStore.h"

#include <QDebug>
#include <QFile>
#include <QSoundEffect>
#include <QUrl>

namespace {

quint16 readLe16(const QByteArray &data, qsizetype offset)
{
    return static_cast<quint8>(data[offset])
        | (static_cast<quint16>(static_cast<quint8>(data[offset + 1])) << 8);
}

quint32 readLe32(const QByteArray &data, qsizetype offset)
{
    return static_cast<quint8>(data[offset])
        | (static_cast<quint32>(static_cast<quint8>(data[offset + 1])) << 8)
        | (static_cast<quint32>(static_cast<quint8>(data[offset + 2])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(data[offset + 3])) << 24);
}

bool isShutdownState(ScootEnums::VehicleState state)
{
    using State = ScootEnums::VehicleState;
    return state == State::ShuttingDown || state == State::Hibernating
        || state == State::HibernatingImminent || state == State::Suspending
        || state == State::SuspendingImminent;
}

QString cueFileName(SoundCue cue)
{
    switch (cue) {
    case SoundCue::Ready: return QStringLiteral("state-ready.wav");
    case SoundCue::Parked: return QStringLiteral("state-parked.wav");
    case SoundCue::Shutdown: return QStringLiteral("state-shutdown.wav");
    case SoundCue::IndicatorOn: return QStringLiteral("indicator-on.wav");
    case SoundCue::IndicatorOff: return QStringLiteral("indicator-off.wav");
    case SoundCue::Info: return QStringLiteral("notification-info.wav");
    case SoundCue::Success: return QStringLiteral("notification-success.wav");
    case SoundCue::Warning: return QStringLiteral("notification-warning.wav");
    case SoundCue::Error: return QStringLiteral("notification-error.wav");
    case SoundCue::None: return {};
    }
    return {};
}

bool isStateCue(SoundCue cue)
{
    return cue == SoundCue::Ready || cue == SoundCue::Parked || cue == SoundCue::Shutdown;
}

} // namespace

SoundEvent SoundCueMapping::vehicleTransition(ScootEnums::VehicleState from,
                                               ScootEnums::VehicleState to)
{
    using State = ScootEnums::VehicleState;
    if (from == to || from == State::Unknown)
        return SoundEvent::None;
    if (to == State::ReadyToDrive)
        return SoundEvent::VehicleReady;
    if (to == State::Parked && from == State::ReadyToDrive)
        return SoundEvent::VehicleParked;
    if (isShutdownState(to) && !isShutdownState(from))
        return SoundEvent::VehicleShutdown;
    return SoundEvent::None;
}

SoundEvent SoundCueMapping::indicatorTransition(ScootEnums::BlinkerState from,
                                                 ScootEnums::BlinkerState to)
{
    using State = ScootEnums::BlinkerState;
    if (from == to)
        return SoundEvent::None;
    if (to == State::Off)
        return SoundEvent::IndicatorOff;
    return SoundEvent::IndicatorOn;
}

SoundEvent SoundCueMapping::notification(const QString &type)
{
    if (type == QLatin1String("info")) return SoundEvent::NotificationInfo;
    if (type == QLatin1String("success")) return SoundEvent::NotificationSuccess;
    if (type == QLatin1String("warning")) return SoundEvent::NotificationWarning;
    if (type == QLatin1String("error")) return SoundEvent::NotificationError;
    return SoundEvent::None;
}

SoundCue SoundCueMapping::cueForEvent(SoundEvent event)
{
    switch (event) {
    case SoundEvent::VehicleReady: return SoundCue::Ready;
    case SoundEvent::VehicleParked: return SoundCue::Parked;
    case SoundEvent::VehicleShutdown: return SoundCue::Shutdown;
    case SoundEvent::IndicatorOn: return SoundCue::IndicatorOn;
    case SoundEvent::IndicatorOff: return SoundCue::IndicatorOff;
    case SoundEvent::NotificationInfo: return SoundCue::Info;
    case SoundEvent::NotificationSuccess: return SoundCue::Success;
    case SoundEvent::NotificationWarning: return SoundCue::Warning;
    case SoundEvent::NotificationError: return SoundCue::Error;
    case SoundEvent::None: return SoundCue::None;
    }
    return SoundCue::None;
}

SoundCueService::SoundCueService(VehicleStore *vehicleStore, ToastService *toastService,
                                 const QString &assetRoot, QObject *parent)
    : QObject(parent)
    , m_vehicleStore(vehicleStore)
    , m_vehicleState(static_cast<ScootEnums::VehicleState>(vehicleStore->state()))
    , m_blinkerState(static_cast<ScootEnums::BlinkerState>(vehicleStore->blinkerState()))
{
    loadCues(assetRoot);

    connect(vehicleStore, &VehicleStore::stateChanged, this, [this]() {
        const auto next = static_cast<ScootEnums::VehicleState>(m_vehicleStore->state());
        playEvent(SoundCueMapping::vehicleTransition(m_vehicleState, next));
        m_vehicleState = next;
    });
    connect(vehicleStore, &VehicleStore::blinkerStateChanged, this, [this]() {
        const auto next = static_cast<ScootEnums::BlinkerState>(m_vehicleStore->blinkerState());
        playEvent(SoundCueMapping::indicatorTransition(m_blinkerState, next));
        m_blinkerState = next;
    });
    connect(toastService, &ToastService::toastAdded, this, [this](const QString &type) {
        playEvent(SoundCueMapping::notification(type));
    });
}

bool SoundCueService::validateWaveFile(const QString &path, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    QString filePath = path;
    if (filePath.startsWith(QLatin1String("qrc:/")))
        filePath.remove(0, 3);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("cannot open file"));
    const QByteArray data = file.readAll();
    if (data.size() < 12 || data.first(4) != "RIFF" || data.sliced(8, 4) != "WAVE")
        return fail(QStringLiteral("not a RIFF/WAVE file"));
    const quint64 riffEnd = static_cast<quint64>(readLe32(data, 4)) + 8;
    if (riffEnd < 12 || riffEnd > static_cast<quint64>(data.size()))
        return fail(QStringLiteral("truncated RIFF data"));

    bool validFormat = false;
    bool hasAudio = false;
    quint64 offset = 12;
    while (offset + 8 <= riffEnd) {
        const auto chunkOffset = static_cast<qsizetype>(offset);
        const QByteArray id = data.sliced(chunkOffset, 4);
        const quint32 size = readLe32(data, chunkOffset + 4);
        const quint64 payload = offset + 8;
        const quint64 paddedEnd = payload + size + (size & 1u);
        if (paddedEnd > riffEnd)
            return fail(QStringLiteral("truncated chunk"));
        if (id == "fmt ") {
            if (size < 16)
                return fail(QStringLiteral("invalid format chunk"));
            const auto formatOffset = static_cast<qsizetype>(payload);
            validFormat = readLe16(data, formatOffset) == 1
                && readLe16(data, formatOffset + 2) == 2
                && readLe32(data, formatOffset + 4) == 48000
                && readLe32(data, formatOffset + 8) == 192000
                && readLe16(data, formatOffset + 12) == 4
                && readLe16(data, formatOffset + 14) == 16;
        } else if (id == "data") {
            hasAudio = size > 0 && size % 4 == 0;
        }
        offset = paddedEnd;
    }

    if (offset != riffEnd)
        return fail(QStringLiteral("truncated chunk header"));
    if (!validFormat)
        return fail(QStringLiteral("expected 48 kHz stereo 16-bit PCM"));
    if (!hasAudio)
        return fail(QStringLiteral("missing PCM audio data"));
    return true;
}

void SoundCueService::loadCues(const QString &assetRoot)
{
    for (int value = static_cast<int>(SoundCue::Ready);
         value <= static_cast<int>(SoundCue::Error); ++value) {
        const auto cue = static_cast<SoundCue>(value);
        const QString path = assetRoot + QLatin1Char('/') + cueFileName(cue);
        QString error;
        if (!validateWaveFile(path, &error)) {
            qWarning() << "Sound cue disabled:" << path << error;
            continue;
        }

        auto *effect = new QSoundEffect(this);
        effect->setVolume(DefaultVolume);
        effect->setSource(QUrl(path));
        connect(effect, &QSoundEffect::statusChanged, this, [effect, path]() {
            if (effect->status() == QSoundEffect::Error)
                qWarning() << "Sound cue playback unavailable:" << path;
        });
        m_effects.insert(cue, effect);
    }
}

void SoundCueService::playEvent(SoundEvent event)
{
    playCue(SoundCueMapping::cueForEvent(event));
}

void SoundCueService::playCue(SoundCue cue)
{
    auto *effect = m_effects.value(cue, nullptr);
    if (!effect)
        return;

    if (isStateCue(cue)) {
        for (auto it = m_effects.cbegin(); it != m_effects.cend(); ++it) {
            if (isStateCue(it.key()) && it.value()->isPlaying())
                it.value()->stop();
        }
    }
    effect->play();
}

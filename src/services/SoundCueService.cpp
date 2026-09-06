#include "SoundCueService.h"

#include "services/ToastService.h"
#include "stores/BatteryStore.h"
#include "stores/VehicleStore.h"

#include <QAudioDevice>
#include <QDebug>
#include <QFile>
#include <QMediaDevices>
#include <QSoundEffect>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace {

constexpr int kCueLoadDelayMs = 3500;

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
    case SoundCue::Wake: return QStringLiteral("scooter-unlock.wav");
    case SoundCue::Ready: return QStringLiteral("vehicle-ready-to-drive.wav");
    case SoundCue::Parked: return QStringLiteral("vehicle-ready-to-drive-to-parked.wav");
    case SoundCue::Shutdown: return QStringLiteral("scooter-lock.wav");
    case SoundCue::BlinkerPulse: return QStringLiteral("blinker-pulse.wav");
    case SoundCue::BlinkerOff: return QStringLiteral("blinker-off.wav");
    case SoundCue::BatteryInsert: return QStringLiteral("battery-inserted.wav");
    case SoundCue::BatteryRemove: return QStringLiteral("battery-removed.wav");
    case SoundCue::SeatboxOpen: return QStringLiteral("seatbox-open.wav");
    case SoundCue::SeatboxClosed: return QStringLiteral("seatbox-closed.wav");
    case SoundCue::Info: return QStringLiteral("toast-info.wav");
    case SoundCue::Success: return QStringLiteral("toast-success.wav");
    case SoundCue::Warning: return QStringLiteral("toast-warning.wav");
    case SoundCue::Error: return QStringLiteral("toast-error.wav");
    case SoundCue::None: return {};
    }
    return {};
}

bool isStateCue(SoundCue cue)
{
    return cue == SoundCue::Wake || cue == SoundCue::Ready
        || cue == SoundCue::Parked || cue == SoundCue::Shutdown;
}

QAudioDevice selectAudioOutput()
{
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    if (outputs.isEmpty())
        return {};

    const QString configured = qEnvironmentVariable("SCOOTUI_AUDIO_DEVICE");
    if (!configured.isEmpty()) {
        for (const auto &device : outputs) {
            if (QString::fromUtf8(device.id()).compare(configured, Qt::CaseInsensitive) == 0
                || device.description().compare(configured, Qt::CaseInsensitive) == 0)
                return device;
        }
        qWarning() << "Configured audio output not found:" << configured;
    }

    for (const auto &device : outputs) {
        const QString identity = QString::fromUtf8(device.id()) + QLatin1Char(' ')
            + device.description();
        if (identity.contains(QLatin1String("tas5720"), Qt::CaseInsensitive))
            return device;
    }
    for (const auto &device : outputs) {
        const QString identity = QString::fromUtf8(device.id()) + QLatin1Char(' ')
            + device.description();
        if (identity.contains(QLatin1String("usb"), Qt::CaseInsensitive))
            return device;
    }

    const QAudioDevice defaultOutput = QMediaDevices::defaultAudioOutput();
    return defaultOutput.isNull() ? outputs.first() : defaultOutput;
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
    if (to == State::Parked && from == State::StandBy)
        return SoundEvent::VehicleWake;
    if (to == State::Parked && from == State::ReadyToDrive)
        return SoundEvent::VehicleParked;
    if (isShutdownState(to) && !isShutdownState(from))
        return SoundEvent::VehicleShutdown;
    return SoundEvent::None;
}

SoundEvent SoundCueMapping::blinkerPhase(qreal previousOpacity, qreal opacity, bool active)
{
    constexpr qreal threshold = 0.02;
    if (!active)
        return SoundEvent::None;
    if (previousOpacity <= threshold && opacity > threshold)
        return SoundEvent::BlinkerPulse;
    return SoundEvent::None;
}

SoundEvent SoundCueMapping::blinkerTransition(ScootEnums::BlinkerState from,
                                               ScootEnums::BlinkerState to)
{
    if (from != ScootEnums::BlinkerState::Off && to == ScootEnums::BlinkerState::Off)
        return SoundEvent::BlinkerOff;
    return SoundEvent::None;
}

SoundEvent SoundCueMapping::batteryPresence(bool wasPresent, bool present)
{
    if (wasPresent == present)
        return SoundEvent::None;
    return present ? SoundEvent::BatteryInserted : SoundEvent::BatteryRemoved;
}

SoundEvent SoundCueMapping::seatboxTransition(ScootEnums::SeatboxLock from,
                                               ScootEnums::SeatboxLock to)
{
    if (from == to)
        return SoundEvent::None;
    return to == ScootEnums::SeatboxLock::Open
        ? SoundEvent::SeatboxOpened : SoundEvent::SeatboxClosed;
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
    case SoundEvent::VehicleWake: return SoundCue::Wake;
    case SoundEvent::VehicleReady: return SoundCue::Ready;
    case SoundEvent::VehicleParked: return SoundCue::Parked;
    case SoundEvent::VehicleShutdown: return SoundCue::Shutdown;
    case SoundEvent::BlinkerPulse: return SoundCue::BlinkerPulse;
    case SoundEvent::BlinkerOff: return SoundCue::BlinkerOff;
    case SoundEvent::BatteryInserted: return SoundCue::BatteryInsert;
    case SoundEvent::BatteryRemoved: return SoundCue::BatteryRemove;
    case SoundEvent::SeatboxOpened: return SoundCue::SeatboxOpen;
    case SoundEvent::SeatboxClosed: return SoundCue::SeatboxClosed;
    case SoundEvent::NotificationInfo: return SoundCue::Info;
    case SoundEvent::NotificationSuccess: return SoundCue::Success;
    case SoundEvent::NotificationWarning: return SoundCue::Warning;
    case SoundEvent::NotificationError: return SoundCue::Error;
    case SoundEvent::None: return SoundCue::None;
    }
    return SoundCue::None;
}

SoundCueService::SoundCueService(VehicleStore *vehicleStore, BatteryStore *battery0Store,
                                 BatteryStore *battery1Store, ToastService *toastService,
                                 const QString &assetRoot, QObject *parent)
    : QObject(parent)
    , m_vehicleStore(vehicleStore)
    , m_battery0Store(battery0Store)
    , m_battery1Store(battery1Store)
    , m_vehicleState(static_cast<ScootEnums::VehicleState>(vehicleStore->state()))
    , m_seatboxState(static_cast<ScootEnums::SeatboxLock>(vehicleStore->seatboxLock()))
    , m_blinkerState(static_cast<ScootEnums::BlinkerState>(vehicleStore->blinkerState()))
{
    QTimer::singleShot(kCueLoadDelayMs, this, [this, assetRoot]() {
        loadCues(assetRoot);
    });

    connect(vehicleStore, &VehicleStore::stateChanged, this, [this]() {
        const auto next = static_cast<ScootEnums::VehicleState>(m_vehicleStore->state());
        if (m_armed)
            playEvent(SoundCueMapping::vehicleTransition(m_vehicleState, next));
        m_vehicleState = next;
    });
    connect(vehicleStore, &VehicleStore::blinkOpacityChanged, this, [this]() {
        const qreal next = m_vehicleStore->blinkOpacity();
        if (m_armed) {
            const bool active = static_cast<ScootEnums::BlinkerState>(
                m_vehicleStore->blinkerState()) != ScootEnums::BlinkerState::Off;
            playEvent(SoundCueMapping::blinkerPhase(m_blinkOpacity, next, active));
        }
        m_blinkOpacity = next;
    });
    connect(vehicleStore, &VehicleStore::blinkerStateChanged, this, [this]() {
        const auto next = static_cast<ScootEnums::BlinkerState>(m_vehicleStore->blinkerState());
        if (m_armed)
            playEvent(SoundCueMapping::blinkerTransition(m_blinkerState, next));
        m_blinkerState = next;
    });
    connect(vehicleStore, &VehicleStore::seatboxLockChanged, this, [this]() {
        const auto next = static_cast<ScootEnums::SeatboxLock>(m_vehicleStore->seatboxLock());
        if (m_armed)
            playEvent(SoundCueMapping::seatboxTransition(m_seatboxState, next));
        m_seatboxState = next;
    });
    connect(battery0Store, &BatteryStore::presentChanged, this, [this]() {
        const bool next = m_battery0Store->present();
        if (m_armed)
            playEvent(SoundCueMapping::batteryPresence(m_battery0Present, next));
        m_battery0Present = next;
    });
    connect(battery1Store, &BatteryStore::presentChanged, this, [this]() {
        const bool next = m_battery1Store->present();
        if (m_armed)
            playEvent(SoundCueMapping::batteryPresence(m_battery1Present, next));
        m_battery1Present = next;
    });
    connect(toastService, &ToastService::toastAdded, this, [this](const QString &type) {
        if (m_armed)
            playEvent(SoundCueMapping::notification(type));
    });
}

void SoundCueService::arm()
{
    m_vehicleState = static_cast<ScootEnums::VehicleState>(m_vehicleStore->state());
    m_seatboxState = static_cast<ScootEnums::SeatboxLock>(m_vehicleStore->seatboxLock());
    m_blinkerState = static_cast<ScootEnums::BlinkerState>(m_vehicleStore->blinkerState());
    m_blinkOpacity = m_vehicleStore->blinkOpacity();
    m_battery0Present = m_battery0Store->present();
    m_battery1Present = m_battery1Store->present();
    m_armed = true;
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
    const QAudioDevice output = selectAudioOutput();
    if (output.isNull()) {
        qInfo() << "Sound cues disabled: no audio output available";
        return;
    }
    m_audioAvailable = true;

    for (int value = static_cast<int>(SoundCue::Wake);
         value <= static_cast<int>(SoundCue::Error); ++value) {
        const auto cue = static_cast<SoundCue>(value);
        const QString path = assetRoot + QLatin1Char('/') + cueFileName(cue);
        QString error;
        if (!validateWaveFile(path, &error)) {
            qWarning() << "Sound cue disabled:" << path << error;
            continue;
        }

        auto *effect = new QSoundEffect(output, this);
        effect->setVolume(DefaultVolume);
        effect->setSource(QUrl(path));
        connect(effect, &QSoundEffect::statusChanged, this, [this, effect, path]() {
            if (effect->status() == QSoundEffect::Error)
                disableAudio(QStringLiteral("playback unavailable: ") + path);
        });
        m_effects.insert(cue, effect);
    }
}

void SoundCueService::disableAudio(const QString &reason)
{
    if (!m_audioAvailable)
        return;
    m_audioAvailable = false;
    for (auto *effect : std::as_const(m_effects)) {
        effect->stop();
        effect->setMuted(true);
    }
    qWarning() << "Sound cues disabled:" << reason;
}

void SoundCueService::playEvent(SoundEvent event)
{
    playCue(SoundCueMapping::cueForEvent(event));
}

void SoundCueService::playCue(SoundCue cue)
{
    if (!m_audioAvailable)
        return;
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

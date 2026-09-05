#include <QtTest>

#include "services/SoundCueService.h"

#include <QFile>
#include <QTemporaryDir>

namespace {

void appendLe16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(QByteArray &data, quint32 value)
{
    appendLe16(data, static_cast<quint16>(value & 0xffff));
    appendLe16(data, static_cast<quint16>(value >> 16));
}

QByteArray waveData(quint16 channels = 2, quint32 sampleRate = 48000,
                    quint16 bitsPerSample = 16)
{
    const quint16 blockAlign = channels * bitsPerSample / 8;
    const quint32 byteRate = sampleRate * blockAlign;
    QByteArray pcm(blockAlign * 8, '\0');
    QByteArray data("RIFF", 4);
    appendLe32(data, 36 + pcm.size());
    data.append("WAVEfmt ", 8);
    appendLe32(data, 16);
    appendLe16(data, 1);
    appendLe16(data, channels);
    appendLe32(data, sampleRate);
    appendLe32(data, byteRate);
    appendLe16(data, blockAlign);
    appendLe16(data, bitsPerSample);
    data.append("data", 4);
    appendLe32(data, pcm.size());
    data.append(pcm);
    return data;
}

QString writeFile(const QTemporaryDir &dir, const QString &name, const QByteArray &data)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size())
        return {};
    return path;
}

int eventValue(SoundEvent event)
{
    return static_cast<int>(event);
}

int cueValue(SoundCue cue)
{
    return static_cast<int>(cue);
}

} // namespace

class SoundCueServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsVehicleTransitions()
    {
        using State = ScootEnums::VehicleState;
        QCOMPARE(eventValue(SoundCueMapping::vehicleTransition(State::Parked, State::ReadyToDrive)),
                 eventValue(SoundEvent::VehicleReady));
        QCOMPARE(eventValue(SoundCueMapping::vehicleTransition(State::StandBy, State::Parked)),
                 eventValue(SoundEvent::VehicleWake));
        QCOMPARE(eventValue(SoundCueMapping::vehicleTransition(State::ReadyToDrive, State::Parked)),
                 eventValue(SoundEvent::VehicleParked));
        QCOMPARE(eventValue(SoundCueMapping::vehicleTransition(State::ReadyToDrive, State::ShuttingDown)),
                 eventValue(SoundEvent::VehicleShutdown));
        QCOMPARE(eventValue(SoundCueMapping::vehicleTransition(State::ShuttingDown, State::Hibernating)),
                 eventValue(SoundEvent::None));
        QCOMPARE(eventValue(SoundCueMapping::vehicleTransition(State::Unknown, State::ReadyToDrive)),
                 eventValue(SoundEvent::None));
        QCOMPARE(eventValue(SoundCueMapping::vehicleTransition(State::Parked, State::HopOn)),
                 eventValue(SoundEvent::None));
    }

    void mapsIndicatorPhases()
    {
        QCOMPARE(eventValue(SoundCueMapping::indicatorPhase(0.0, 0.2, true)),
                 eventValue(SoundEvent::IndicatorOn));
        QCOMPARE(eventValue(SoundCueMapping::indicatorPhase(0.8, 0.0, true)),
                 eventValue(SoundEvent::IndicatorOff));
        QCOMPARE(eventValue(SoundCueMapping::indicatorPhase(0.2, 0.8, true)),
                 eventValue(SoundEvent::None));
        QCOMPARE(eventValue(SoundCueMapping::indicatorPhase(0.8, 0.0, false)),
                 eventValue(SoundEvent::None));
    }

    void mapsBatteryAndSeatboxTransitions()
    {
        QCOMPARE(eventValue(SoundCueMapping::batteryPresence(false, true)),
                 eventValue(SoundEvent::BatteryInserted));
        QCOMPARE(eventValue(SoundCueMapping::batteryPresence(true, false)),
                 eventValue(SoundEvent::BatteryRemoved));
        QCOMPARE(eventValue(SoundCueMapping::batteryPresence(true, true)),
                 eventValue(SoundEvent::None));
        QCOMPARE(eventValue(SoundCueMapping::seatboxTransition(
                     ScootEnums::SeatboxLock::Closed, ScootEnums::SeatboxLock::Open)),
                 eventValue(SoundEvent::SeatboxOpened));
        QCOMPARE(eventValue(SoundCueMapping::seatboxTransition(
                     ScootEnums::SeatboxLock::Open, ScootEnums::SeatboxLock::Closed)),
                 eventValue(SoundEvent::SeatboxClosed));
    }

    void mapsNotificationsAndCues()
    {
        QCOMPARE(eventValue(SoundCueMapping::notification(QStringLiteral("info"))),
                 eventValue(SoundEvent::NotificationInfo));
        QCOMPARE(eventValue(SoundCueMapping::notification(QStringLiteral("success"))),
                 eventValue(SoundEvent::NotificationSuccess));
        QCOMPARE(eventValue(SoundCueMapping::notification(QStringLiteral("warning"))),
                 eventValue(SoundEvent::NotificationWarning));
        QCOMPARE(eventValue(SoundCueMapping::notification(QStringLiteral("error"))),
                 eventValue(SoundEvent::NotificationError));
        QCOMPARE(eventValue(SoundCueMapping::notification(QStringLiteral("other"))),
                 eventValue(SoundEvent::None));
        QCOMPARE(cueValue(SoundCueMapping::cueForEvent(SoundEvent::VehicleReady)),
                 cueValue(SoundCue::Ready));
        QCOMPARE(cueValue(SoundCueMapping::cueForEvent(SoundEvent::NotificationError)),
                 cueValue(SoundCue::Error));
        QCOMPARE(cueValue(SoundCueMapping::cueForEvent(SoundEvent::None)),
                 cueValue(SoundCue::None));
    }

    void generatedAssetsMeetFormat()
    {
        const QStringList names = {
            QStringLiteral("battery-insert.wav"),
            QStringLiteral("battery-remove.wav"),
            QStringLiteral("seatbox-open.wav"),
            QStringLiteral("seatbox-closed.wav"),
            QStringLiteral("state-wake.wav"),
            QStringLiteral("state-ready.wav"),
            QStringLiteral("state-parked.wav"),
            QStringLiteral("state-shutdown.wav"),
            QStringLiteral("indicator-on.wav"),
            QStringLiteral("indicator-off.wav"),
            QStringLiteral("notification-info.wav"),
            QStringLiteral("notification-success.wav"),
            QStringLiteral("notification-warning.wav"),
            QStringLiteral("notification-error.wav"),
        };
        for (const QString &name : names) {
            const QString path = QStringLiteral(SOUND_ASSET_DIR) + QLatin1Char('/') + name;
            QVERIFY2(SoundCueService::validateWaveFile(path), qPrintable(path));

            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QByteArray data = file.readAll();
            for (qsizetype offset = 44; offset + 3 < data.size(); offset += 4) {
                QCOMPARE(data.sliced(offset, 2), data.sliced(offset + 2, 2));
            }
        }
    }

    void validatesEmbeddedAsset()
    {
        QVERIFY(SoundCueService::validateWaveFile(
            QStringLiteral("qrc:/ScootUI/assets/sounds/state-ready.wav")));
    }

    void validatesWaveAssets()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString error;
        QVERIFY(SoundCueService::validateWaveFile(writeFile(dir, QStringLiteral("valid.wav"), waveData()), &error));
        QVERIFY(!SoundCueService::validateWaveFile(dir.filePath(QStringLiteral("missing.wav")), &error));
        QCOMPARE(error, QStringLiteral("cannot open file"));
        QVERIFY(!SoundCueService::validateWaveFile(
            writeFile(dir, QStringLiteral("invalid.wav"), QByteArray("not wave")), &error));
        QCOMPARE(error, QStringLiteral("not a RIFF/WAVE file"));
        QVERIFY(!SoundCueService::validateWaveFile(
            writeFile(dir, QStringLiteral("mono.wav"), waveData(1)), &error));
        QCOMPARE(error, QStringLiteral("expected 48 kHz stereo 16-bit PCM"));
        QVERIFY(!SoundCueService::validateWaveFile(
            writeFile(dir, QStringLiteral("low-rate.wav"), waveData(2, 44100)), &error));
        QCOMPARE(error, QStringLiteral("expected 48 kHz stereo 16-bit PCM"));

        QByteArray oversized = waveData();
        oversized.replace(4, 4, QByteArray(4, static_cast<char>(0xff)));
        QVERIFY(!SoundCueService::validateWaveFile(
            writeFile(dir, QStringLiteral("oversized.wav"), oversized), &error));
        QCOMPARE(error, QStringLiteral("truncated RIFF data"));
    }
};

QTEST_APPLESS_MAIN(SoundCueServiceTest)
#include "SoundCueServiceTest.moc"

#include "UpdateChannelService.h"

#include "services/SettingsService.h"
#include "stores/InternetStore.h"
#include "stores/OtaStore.h"
#include "stores/SettingsStore.h"
#include "services/SystemInfoService.h"
#include "models/Enums.h"

#include <QDebug>
#include <QUuid>

namespace {

// A preview needs both components to answer. update-service bounds its own
// fetch at 20s, so this is that plus room for the Redis round trips on either
// side; past it, whatever has not answered is not going to.
constexpr int kPreviewTimeoutMs = 25000;

// Same rule update-service applies when no channel is configured: the tag the
// image was built from says which stream it came off. Kept here so the confirm
// screen can name the channel the rider is leaving even when the (transient)
// channel setting is absent, which is the normal state after a reboot.
QString channelFromVersion(const QString &versionId)
{
    const QString v = versionId.section(QLatin1Char(' '), 0, 0);
    if (v.startsWith(QLatin1String("nightly-")))
        return QStringLiteral("nightly");
    if (v.startsWith(QLatin1String("testing-")))
        return QStringLiteral("testing");
    if (v.startsWith(QLatin1Char('v')) || (!v.isEmpty() && v.at(0).isDigit()))
        return QStringLiteral("stable");
    return {};
}

} // namespace

UpdateChannelService::UpdateChannelService(SettingsService *settingsService, SettingsStore *settings,
                                           OtaStore *ota, InternetStore *internet,
                                           SystemInfoService *systemInfo, QObject *parent)
    : QObject(parent)
    , m_settingsService(settingsService)
    , m_settings(settings)
    , m_ota(ota)
    , m_internet(internet)
    , m_systemInfo(systemInfo)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(kPreviewTimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        if (m_state != Checking)
            return;
        // Nothing more is coming. Not being able to price the download is not
        // a reason to refuse the switch, so Failed still offers Confirm; it
        // just stops promising a number nobody produced.
        qWarning() << "[UpdateChannelService] request timed out for" << m_targetChannel;
        const bool wasConfirmation = m_waitingConfirmation;
        m_waitingConfirmation = false;
        setState(wasConfirmation ? RequestFailed : Failed);
    });

    m_evaluate.setSingleShot(true);
    m_evaluate.setInterval(0);
    connect(&m_evaluate, &QTimer::timeout, this, &UpdateChannelService::evaluatePreviews);

    if (m_ota) {
        auto schedule = [this]() { m_evaluate.start(); };
        connect(m_ota, &OtaStore::mdbPreviewChanged, this, schedule);
        connect(m_ota, &OtaStore::dbcPreviewChanged, this, schedule);
        connect(m_ota, &OtaStore::switchChanged, this, &UpdateChannelService::evaluateSwitch);
    }
}

QString UpdateChannelService::currentChannel() const
{
    if (m_settings && !m_settings->otaChannel().isEmpty())
        return m_settings->otaChannel();
    // updates.*.channel is transient: after a reboot it is unset and
    // update-service is running on the channel it inferred from the image.
    if (m_systemInfo)
        return channelFromVersion(m_systemInfo->mdbVersionId());
    return {};
}

bool UpdateChannelService::isUpdateInProgress() const
{
    return m_ota && m_ota->isActive();
}

bool UpdateChannelService::isOnline() const
{
    return m_internet
           && m_internet->modemState() == static_cast<int>(ScootEnums::ModemState::Connected);
}

// Always emits, even when the state value is unchanged: version and
// totalBytes hang off the same signal, and a re-entered Checking with cleared
// numbers is a change the screen has to see.
void UpdateChannelService::setState(State s)
{
    m_state = s;
    emit stateChanged();
}

void UpdateChannelService::beginSwitch(const QString &channel)
{
    m_timeout.stop();
    m_evaluate.stop();
    m_version.clear();
    m_totalBytes = 0;
    m_partial = false;
    m_waitingConfirmation = false;
    m_requestId.clear();

    if (m_targetChannel != channel) {
        m_targetChannel = channel;
        emit targetChannelChanged();
    }

    if (!isOnline()) {
        // No preview to ask for, and nothing to download either way. The
        // screen explains the .mender-over-Update-Mode route instead.
        setState(Offline);
        return;
    }

    setState(Checking);
    if (m_settingsService)
        m_settingsService->requestChannelPreview(channel);
    m_timeout.start();
}

// evaluatePreviews maps the two components' answers onto one state. Answers
// carrying a different channel than the one being asked about are ignored
// rather than mixed in: a preview for the channel the rider looked at a
// moment ago is not an answer about this one.
void UpdateChannelService::evaluatePreviews()
{
    if (m_state != Checking || m_waitingConfirmation || !m_ota)
        return;

    struct Answer { QString status; QString version; qint64 size; };
    const Answer answers[] = {
        {m_ota->mdbPreviewChannel() == m_targetChannel ? m_ota->mdbPreviewStatus() : QString(),
         m_ota->mdbPreviewVersion(), m_ota->mdbPreviewMethod().isEmpty() ? m_ota->mdbPreviewSize() : m_ota->mdbPreviewDownloadSize()},
        {m_ota->dbcPreviewChannel() == m_targetChannel ? m_ota->dbcPreviewStatus() : QString(),
         m_ota->dbcPreviewVersion(), m_ota->dbcPreviewMethod().isEmpty() ? m_ota->dbcPreviewSize() : m_ota->dbcPreviewDownloadSize()},
    };

    bool anyError = false;
    bool allUnavailable = true;
    qint64 total = 0;
    QString version;

    for (const Answer &a : answers) {
        if (a.status.isEmpty() || a.status == QLatin1String("checking"))
            return; // still waiting; the timeout is the backstop
        if (a.status == QLatin1String("error")) {
            anyError = true;
            continue;
        }
        if (a.status == QLatin1String("unavailable"))
            continue;
        allUnavailable = false;
        total += a.size;
        if (version.isEmpty()) {
            version = a.version;
        } else if (version != a.version) {
            version = QStringLiteral("MDB: ") + answers[0].version + QStringLiteral("\nDBC: ") + answers[1].version;
        }
    }

    m_timeout.stop();

    if (anyError) {
        setState(Failed);
        return;
    }
    if (allUnavailable) {
        setState(Unavailable);
        return;
    }

    m_partial = (answers[0].status == QLatin1String("unavailable")) != (answers[1].status == QLatin1String("unavailable"));
    if (m_partial) {
        const QString unavailable = QStringLiteral("-");
        version = QStringLiteral("MDB: ") + (answers[0].version.isEmpty() ? unavailable : answers[0].version)
                  + QStringLiteral("\nDBC: ") + (answers[1].version.isEmpty() ? unavailable : answers[1].version);
    }
    m_version = version;
    m_totalBytes = total;
    setState(Ready);
}

void UpdateChannelService::confirm()
{
    m_timeout.stop();
    m_evaluate.stop();
    if (!m_settingsService || m_targetChannel.isEmpty()) {
        setState(Failed);
        return;
    }
    m_requestId = QUuid::createUuid().toString(QUuid::Id128);
    m_waitingConfirmation = true;
    setState(Checking);
    m_settingsService->requestChannelSwitch(m_targetChannel, m_requestId);
    m_timeout.start();
}

void UpdateChannelService::evaluateSwitch()
{
    if (!m_waitingConfirmation || !m_ota)
        return;
    const QString prefix = m_requestId + QLatin1Char(':');
    if (!m_ota->switchResult().startsWith(prefix))
        return;
    const QString status = m_ota->switchResult().mid(prefix.size());
    if (status == QLatin1String("accepted") || status == QLatin1String("complete") || status == QLatin1String("partial")) {
        m_timeout.stop();
        m_waitingConfirmation = false;
        setState(Idle);
        emit switchConfirmed();
    } else if (status == QLatin1String("busy") || status == QLatin1String("error") || status == QLatin1String("failed")) {
        m_timeout.stop();
        m_waitingConfirmation = false;
        setState(RequestFailed);
    }
}

void UpdateChannelService::cancel()
{
    m_waitingConfirmation = false;
    m_timeout.stop();
    m_evaluate.stop();
    setState(Idle);
}

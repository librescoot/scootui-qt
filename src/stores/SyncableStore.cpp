#include "SyncableStore.h"

#include <QDebug>

SyncableStore::SyncableStore(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
}

SyncableStore::~SyncableStore()
{
    stop();
}

void SyncableStore::start()
{
    if (m_started) return;
    m_started = true;

    const auto settings = syncSettings();
    m_channel = settings.channel;

    // Register this channel for polling by the worker thread
    m_repo->registerPollChannel(settings.channel, settings.intervalMs);

    // Listen for pushed field updates from the worker
    connect(m_repo, &MdbRepository::fieldsUpdated,
            this, &SyncableStore::onFieldsReceived);

    // Listen for single-field fetches (from HGET after pub/sub notification)
    connect(m_repo, &MdbRepository::fieldFetched,
            this, &SyncableStore::onFieldFetched);

    // Subscribe to pubsub for this channel (triggers immediate HGET in worker)
    m_repo->subscribe(settings.channel, [this](const QString &ch, const QString &msg) {
        onPubsubMessage(ch, msg);
    });

    // Set up set field timers
    for (const auto &field : settings.setFields) {
        doRefreshSet(field);
        scheduleSetTimer(field);
    }
}

void SyncableStore::stop()
{
    if (!m_started) return;
    m_started = false;

    disconnect(m_repo, &MdbRepository::fieldsUpdated,
               this, &SyncableStore::onFieldsReceived);
    disconnect(m_repo, &MdbRepository::fieldFetched,
               this, &SyncableStore::onFieldFetched);

    if (!m_channel.isEmpty())
        m_repo->unsubscribe(m_channel);

    for (auto *timer : m_setTimers) {
        timer->stop();
        delete timer;
    }
    m_setTimers.clear();
}

void SyncableStore::onFieldsReceived(const QString &channel, const FieldMap &fields)
{
    const auto settings = syncSettings();
    if (channel != settings.channel) return;

    for (const auto &field : settings.fields) {
        const auto it = fields.constFind(field.variable);
        if (it != fields.constEnd()) {
            applyFieldUpdate(field.variable, *it);
        } else if (field.clearable) {
            applyFieldUpdate(field.variable, QString());
        }
    }
}

void SyncableStore::onFieldFetched(const QString &channel, const QString &field, const QString &value)
{
    if (!m_started) return;
    if (channel != syncSettings().channel) return;
    applyFieldUpdate(field, value);
}

void SyncableStore::onPubsubMessage(const QString &channel, const QString &message)
{
    if (!m_started) return;

    const auto settings = syncSettings();

    // Check if this is a set field notification
    for (const auto &setField : settings.setFields) {
        if (setField.name == message) {
            doRefreshSet(setField);
            return;
        }
    }

    // For regular field notifications, fetch the changed field immediately
    // so we don't wait up to the full polling interval. The "*" message
    // comes from the worker's own poll (already handled in onFieldsReceived).
    if (message != QLatin1String("*"))
        m_repo->requestField(channel, message);
}

void SyncableStore::refreshAllFields()
{
    // Read from cache (non-blocking)
    const auto settings = syncSettings();
    FieldMap fields = m_repo->getAll(settings.channel);
    onFieldsReceived(settings.channel, fields);

    for (const auto &field : settings.setFields)
        doRefreshSet(field);
}

void SyncableStore::doRefreshSet(const SyncSetFieldDef &field)
{
    if (!m_started) return;

    const QString key = interpolateKey(field.setKey);
    const QStringList members = m_repo->getSetMembers(key);
    applySetUpdate(field.name, members);
}

void SyncableStore::scheduleSetTimer(const SyncSetFieldDef &field)
{
    const auto settings = syncSettings();
    const int interval = (field.intervalMs > 0) ? field.intervalMs : settings.intervalMs;

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, field]() {
        doRefreshSet(field);
    });
    timer->start(interval);

    if (m_setTimers.contains(field.name)) {
        m_setTimers[field.name]->stop();
        delete m_setTimers[field.name];
    }
    m_setTimers[field.name] = timer;
}

void SyncableStore::applySetUpdate(const QString &, const QStringList &)
{
}

QString SyncableStore::interpolateKey(const QString &key) const
{
    if (key.contains(QLatin1Char('$'))) {
        const auto settings = syncSettings();
        if (!settings.discriminator.isEmpty()) {
            const QString discValue = discriminatorValue();
            return QString(key).replace(
                QLatin1Char('$') + settings.discriminator, discValue);
        }
    }
    return key;
}

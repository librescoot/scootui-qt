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

    // Listen for connection state changes
    connect(m_repo, &MdbRepository::connectionStateChanged,
            this, [this](bool connected) {
        if (connected) {
            resumePolling();
        } else {
            pausePolling();
        }
    });

    // Initial fetch
    doHgetall();
    startPollTimer();

    // Set up set field timers
    for (const auto &field : settings.setFields) {
        doRefreshSet(field);
        scheduleSetTimer(field);
    }

    // Subscribe to pubsub
    m_repo->subscribe(settings.channel, [this](const QString &ch, const QString &msg) {
        onPubsubMessage(ch, msg);
    });
}

void SyncableStore::stop()
{
    if (!m_started && m_channel.isEmpty()) return;
    m_isClosing = true;
    m_started = false;

    if (m_pollTimer) {
        m_pollTimer->stop();
        delete m_pollTimer;
        m_pollTimer = nullptr;
    }
    if (m_pubsubDebounce) {
        m_pubsubDebounce->stop();
        delete m_pubsubDebounce;
        m_pubsubDebounce = nullptr;
    }
    for (auto *timer : m_setTimers) {
        timer->stop();
        delete timer;
    }
    m_setTimers.clear();

    if (!m_channel.isEmpty()) {
        m_repo->unsubscribe(m_channel);
    }
}

void SyncableStore::doHgetall()
{
    if (m_isPaused || m_isClosing) return;

    const auto settings = syncSettings();
    const FieldMap values = m_repo->getAll(settings.channel);

    if (m_hasLoggedError) {
        qDebug() << "SyncableStore (" << settings.channel << "): Connection recovered";
        m_hasLoggedError = false;
    }

    // Apply each known field
    for (const auto &field : settings.fields) {
        const auto it = values.constFind(field.variable);
        if (it != values.constEnd()) {
            applyFieldUpdate(field.variable, *it);
        } else if (field.clearable) {
            applyFieldUpdate(field.variable, QString());
        }
    }
}

void SyncableStore::onPubsubMessage(const QString &channel, const QString &message)
{
    if (m_isPaused || m_isClosing) return;

    const auto settings = syncSettings();

    // Check if this is a set field notification
    for (const auto &setField : settings.setFields) {
        if (setField.name == message) {
            doRefreshSet(setField);
            return;
        }
    }

    // Debounce: coalesce rapid pubsub messages into single HGETALL
    if (!m_pubsubDebounce) {
        m_pubsubDebounce = new QTimer(this);
        m_pubsubDebounce->setSingleShot(true);
        connect(m_pubsubDebounce, &QTimer::timeout, this, [this]() {
            doHgetall();
            startPollTimer(); // Reset periodic timer
        });
    }
    m_pubsubDebounce->start(50);
}

void SyncableStore::startPollTimer()
{
    const auto settings = syncSettings();

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &SyncableStore::doHgetall);
    }
    m_pollTimer->start(settings.intervalMs);
}

void SyncableStore::doRefreshSet(const SyncSetFieldDef &field)
{
    if (m_isPaused || m_isClosing) return;

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

    // Clean up any existing timer
    if (m_setTimers.contains(field.name)) {
        m_setTimers[field.name]->stop();
        delete m_setTimers[field.name];
    }
    m_setTimers[field.name] = timer;
}

void SyncableStore::pausePolling()
{
    if (m_isPaused) return;
    m_isPaused = true;

    if (m_pollTimer) m_pollTimer->stop();
    if (m_pubsubDebounce) m_pubsubDebounce->stop();

    for (auto *timer : m_setTimers) {
        timer->stop();
    }

    const auto settings = syncSettings();
    m_repo->unsubscribe(settings.channel);

    qDebug() << "SyncableStore (" << settings.channel << "): Polling paused";
}

#include <QRandomGenerator>

void SyncableStore::resumePolling()
{
    if (!m_isPaused) return;
    m_isPaused = false;
    m_hasLoggedError = false;

    const auto settings = syncSettings();
    qDebug() << "SyncableStore (" << settings.channel << "): Polling resumed (staggered)";

    // Stagger resume to avoid thundering herd on the main thread and network.
    // This is especially important on slow PPP backup connections.
    int delay = 10 + QRandomGenerator::global()->bounded(500);
    QTimer::singleShot(delay, this, [this]() {
        if (m_isPaused || m_isClosing) return;

        const auto settings = syncSettings();
        doHgetall();
        startPollTimer();

        for (const auto &field : settings.setFields) {
            doRefreshSet(field);
            scheduleSetTimer(field);
        }

        m_repo->subscribe(settings.channel, [this](const QString &ch, const QString &msg) {
            onPubsubMessage(ch, msg);
        });
    });
}

void SyncableStore::refreshAllFields()
{
    doHgetall();
    const auto settings = syncSettings();
    for (const auto &field : settings.setFields) {
        doRefreshSet(field);
    }
}

void SyncableStore::applySetUpdate(const QString &, const QStringList &)
{
    // Default: no-op. Override in stores that have set fields.
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

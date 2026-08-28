#include "InMemoryMdbRepository.h"

#include <QDebug>
#include <QRandomGenerator>

InMemoryMdbRepository::InMemoryMdbRepository(QObject *parent)
    : MdbRepository(parent)
{
    startBrightnessSimulation();
}

InMemoryMdbRepository::~InMemoryMdbRepository()
{
    if (m_brightnessTimer) {
        m_brightnessTimer->stop();
    }
}

void InMemoryMdbRepository::startBrightnessSimulation()
{
    set(QStringLiteral("dashboard"), QStringLiteral("brightness"),
        QStringLiteral("20.0"), false);

    m_brightnessTimer = new QTimer(this);
    connect(m_brightnessTimer, &QTimer::timeout, this, [this]() {
        const double brightness = 5.0 + QRandomGenerator::global()->generateDouble() * 45.0;
        set(QStringLiteral("dashboard"), QStringLiteral("brightness"),
            QString::number(brightness, 'f', 1));
        qDebug() << "InMemory: Simulated brightness:" << brightness;
    });
    m_brightnessTimer->start(10000);
}

QString InMemoryMdbRepository::get(const QString &channel, const QString &variable)
{
    return m_storage.value(channel).value(variable);
}

FieldMap InMemoryMdbRepository::getAll(const QString &channel)
{
    return m_storage.value(channel);
}

void InMemoryMdbRepository::requestAll(const QString &channel)
{
    if (m_storage.contains(channel))
        emit fieldsUpdated(channel, m_storage.value(channel));
}

void InMemoryMdbRepository::set(const QString &channel, const QString &variable,
                                 const QString &value, bool publish)
{
    m_storage[channel][variable] = value;
    if (publish) {
        notifySubscribers(channel, variable);
        emit fieldsUpdated(channel, m_storage.value(channel));
    }
}

void InMemoryMdbRepository::publish(const QString &channel, const QString &message)
{
    notifySubscribers(channel, message);
}

SubscriptionId InMemoryMdbRepository::subscribe(const QString &channel,
                                                   SubscriptionCallback callback)
{
    const SubscriptionId id = m_nextSubscriptionId++;
    m_subscribers[channel].append({id, std::move(callback)});
    return id;
}

void InMemoryMdbRepository::unsubscribe(const QString &channel, SubscriptionId id)
{
    auto it = m_subscribers.find(channel);
    if (it == m_subscribers.end())
        return;
    auto &entries = it.value();
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).id == id) {
            entries.removeAt(i);
            break;
        }
    }
    if (entries.isEmpty())
        m_subscribers.erase(it);
}

void InMemoryMdbRepository::push(const QString &channel, const QString &command)
{
    // Simulate MDB command handling
    if (channel == QLatin1String("scooter:blinker")) {
        set(QStringLiteral("vehicle"), QStringLiteral("blinker:state"), command);
    }
}

void InMemoryMdbRepository::dashboardReady()
{
    set(QStringLiteral("dashboard"), QStringLiteral("ready"), QStringLiteral("true"));
}

void InMemoryMdbRepository::publishButtonEvent(const QString &event)
{
    notifySubscribers(QStringLiteral("buttons"), event);
}

QStringList InMemoryMdbRepository::getSetMembers(const QString &setKey)
{
    return m_setStorage.value(setKey).values();
}

void InMemoryMdbRepository::addToSet(const QString &setKey, const QString &member)
{
    m_setStorage[setKey].insert(member);
}

void InMemoryMdbRepository::removeFromSet(const QString &setKey, const QString &member)
{
    if (m_setStorage.contains(setKey)) {
        m_setStorage[setKey].remove(member);
        if (m_setStorage[setKey].isEmpty()) {
            m_setStorage.remove(setKey);
        }
    }
}

void InMemoryMdbRepository::hdel(const QString &key, const QString &field)
{
    if (m_storage.contains(key) && m_storage[key].contains(field)) {
        m_storage[key].remove(field);
        qDebug() << "InMemory: HDEL" << key << field;
        notifySubscribers(key, field);
    }
}

QStringList InMemoryMdbRepository::lrange(const QString &key, int start, int stop)
{
    Q_UNUSED(key) Q_UNUSED(start) Q_UNUSED(stop)
    return {};
}

QVariantList InMemoryMdbRepository::xrevrange(const QString &key, int count)
{
    Q_UNUSED(key) Q_UNUSED(count)
    return {};
}

void InMemoryMdbRepository::notifySubscribers(const QString &channel, const QString &variable)
{
    const auto it = m_subscribers.constFind(channel);
    if (it != m_subscribers.constEnd()) {
        const auto entries = *it;
        for (const auto &entry : entries)
            entry.callback(channel, variable);
    }
}

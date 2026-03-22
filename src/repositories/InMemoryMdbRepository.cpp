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

void InMemoryMdbRepository::subscribe(const QString &channel, SubscriptionCallback callback)
{
    m_subscribers[channel].append(callback);
}

void InMemoryMdbRepository::unsubscribe(const QString &channel)
{
    m_subscribers.remove(channel);
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

void InMemoryMdbRepository::notifySubscribers(const QString &channel, const QString &variable)
{
    const auto it = m_subscribers.constFind(channel);
    if (it != m_subscribers.constEnd()) {
        for (const auto &callback : *it) {
            callback(channel, variable);
        }
    }
}

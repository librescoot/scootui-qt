#include "InMemoryMdbRepository.h"
#include "services/DestinationRpc.h"
#include "services/SavedLocationsService.h"

#include <QDebug>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QUuid>

namespace {

QString destinationRecordPrefix(int id)
{
    return QStringLiteral("dashboard.saved-locations.%1").arg(id);
}

QString destinationTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

} // namespace

InMemoryMdbRepository::InMemoryMdbRepository(QObject *parent)
    : MdbRepository(parent)
{
    m_valueStorage.insert(QStringLiteral("trip:ready"), QStringLiteral("1"));
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

void InMemoryMdbRepository::requestValue(const QString &key)
{
    emit valueFetched(key, m_valueStorage.value(key));
}

void InMemoryMdbRepository::setValue(const QString &key, const QString &value)
{
    if (value.isEmpty())
        m_valueStorage.remove(key);
    else
        m_valueStorage.insert(key, value);
    emit valueFetched(key, value);
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
    QTimer::singleShot(0, this, [this, channel]() { emit subscriptionReady(channel); });
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
    if (channel == QLatin1String("settings:route-plan")) {
        handleRoutePlanCall(command);
        return;
    }
    if (channel == QLatin1String(DestinationRpc::RequestChannel)) {
        handleDestinationCall(command);
        return;
    }
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

void InMemoryMdbRepository::handleDestinationCall(const QString &envelopeJson)
{
    const QJsonObject envelope = QJsonDocument::fromJson(envelopeJson.toUtf8()).object();
    const QString replyChannel = envelope.value(QStringLiteral("reply_channel")).toString();
    const qint64 deadline = qint64(envelope.value(QStringLiteral("deadline")).toDouble());
    // Expired requests are dropped without a reply, like the Go dispatcher.
    if (deadline > 0 && QDateTime::currentMSecsSinceEpoch() > deadline)
        return;

    const QString method = envelope.value(QStringLiteral("method")).toString();
    const QJsonObject payload = envelope.value(QStringLiteral("payload")).toObject();
    bool ok = false;
    QVariantMap result;
    QString error;
    if (method == QLatin1String("destination.save")) {
        ok = destinationSave(payload, result, error);
    } else if (method == QLatin1String("destination.delete")) {
        ok = destinationDelete(payload, error);
    } else if (method == QLatin1String("destination.touch")) {
        ok = destinationTouch(payload, error);
    } else {
        error = QStringLiteral("unknown method: %1").arg(method);
    }

    QJsonObject reply;
    reply.insert(QStringLiteral("ok"), ok);
    if (ok)
        reply.insert(QStringLiteral("payload"), QJsonObject::fromVariantMap(result));
    else
        reply.insert(QStringLiteral("error"), error);
    publish(replyChannel,
            QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
}

bool InMemoryMdbRepository::destinationSave(const QJsonObject &payload, QVariantMap &result,
                                            QString &error)
{
    int slot = -1;
    if (payload.contains(QStringLiteral("id"))) {
        slot = payload.value(QStringLiteral("id")).toInt(-1);
        if (slot < 0 || slot >= SavedLocationsService::MaxLocations) {
            error = QStringLiteral("location id %1 out of range").arg(slot);
            return false;
        }
    }

    FieldMap &settings = m_storage[QStringLiteral("settings")];
    if (slot < 0) {
        for (int i = 0; i < SavedLocationsService::MaxLocations; ++i) {
            if (settings.value(destinationRecordPrefix(i) + QStringLiteral(".latitude")).isEmpty()) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            error = QStringLiteral("all saved-location slots are full");
            return false;
        }
    }

    const QString prefix = destinationRecordPrefix(slot);
    QString uuid = settings.value(prefix + QStringLiteral(".uuid"));
    if (QUuid(uuid).isNull())
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString now = destinationTimestamp();
    QString createdAt = settings.value(prefix + QStringLiteral(".created-at"));
    if (createdAt.isEmpty())
        createdAt = now;

    settings.insert(prefix + QStringLiteral(".latitude"),
                    QString::number(payload.value(QStringLiteral("latitude")).toDouble(), 'f', 7));
    settings.insert(prefix + QStringLiteral(".longitude"),
                    QString::number(payload.value(QStringLiteral("longitude")).toDouble(), 'f', 7));
    settings.insert(prefix + QStringLiteral(".label"),
                    payload.value(QStringLiteral("label")).toString());
    settings.insert(prefix + QStringLiteral(".uuid"), uuid);
    settings.insert(prefix + QStringLiteral(".created-at"), createdAt);
    settings.insert(prefix + QStringLiteral(".last-used-at"), now);
    publish(QStringLiteral("settings"), prefix);

    result.insert(QStringLiteral("id"), slot);
    result.insert(QStringLiteral("uuid"), uuid);
    return true;
}

bool InMemoryMdbRepository::destinationDelete(const QJsonObject &payload, QString &error)
{
    const int slot = payload.value(QStringLiteral("id")).toInt(-1);
    if (slot < 0 || slot >= SavedLocationsService::MaxLocations) {
        error = QStringLiteral("location id %1 out of range").arg(slot);
        return false;
    }

    FieldMap &settings = m_storage[QStringLiteral("settings")];
    const QString prefix = destinationRecordPrefix(slot);
    const QString uuid = settings.value(prefix + QStringLiteral(".uuid"));
    if (!uuid.isEmpty()) {
        // Drop every destination token for this record from the shortcut-menu
        // items list; the token shape mirrors settings-service's validator.
        const QString itemsKey = QStringLiteral("dashboard.shortcut-menu.items");
        const QString raw = settings.value(itemsKey);
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (!raw.isEmpty() && doc.isArray()) {
            QStringList items;
            bool allStrings = true;
            for (const QJsonValue &value : doc.array()) {
                if (!value.isString()) {
                    allStrings = false;
                    break;
                }
                items.append(value.toString());
            }
            if (allStrings) {
                QStringList kept;
                for (const QString &item : items) {
                    const QStringList parts = item.split(QLatin1Char(':'));
                    if (parts.size() == 3 && parts.at(0) == QLatin1String("destination")
                        && parts.at(1).compare(uuid, Qt::CaseInsensitive) == 0) {
                        continue;
                    }
                    kept.append(item);
                }
                if (kept.size() != items.size()) {
                    settings.insert(itemsKey, QString::fromUtf8(
                        QJsonDocument(QJsonArray::fromStringList(kept))
                            .toJson(QJsonDocument::Compact)));
                    publish(QStringLiteral("settings"), itemsKey);
                }
            }
        }
    }

    const QString recordLead = prefix + QLatin1Char('.');
    for (auto it = settings.begin(); it != settings.end();) {
        if (it.key().startsWith(recordLead))
            it = settings.erase(it);
        else
            ++it;
    }
    publish(QStringLiteral("settings"), prefix);
    return true;
}

bool InMemoryMdbRepository::destinationTouch(const QJsonObject &payload, QString &error)
{
    const int slot = payload.value(QStringLiteral("id")).toInt(-1);
    if (slot < 0 || slot >= SavedLocationsService::MaxLocations) {
        error = QStringLiteral("location id %1 out of range").arg(slot);
        return false;
    }

    FieldMap &settings = m_storage[QStringLiteral("settings")];
    const QString prefix = destinationRecordPrefix(slot);
    if (settings.value(prefix + QStringLiteral(".latitude")).isEmpty()) {
        error = QStringLiteral("location %1 not found").arg(slot);
        return false;
    }
    const QString field = prefix + QStringLiteral(".last-used-at");
    settings.insert(field, destinationTimestamp());
    publish(QStringLiteral("settings"), field);
    return true;
}

void InMemoryMdbRepository::publishRoutePlan()
{
    const QString channel = QStringLiteral("navigation");
    const QJsonArray stops = m_routePlan.value(QStringLiteral("stops")).toArray();
    const int step = m_routePlan.value(QStringLiteral("current_step")).toInt();
    QJsonArray waypoints;
    for (const QJsonValue &value : stops) {
        const QJsonObject item = value.toObject();
        waypoints.append(QJsonObject{{QStringLiteral("lat"), item.value(QStringLiteral("lat"))},
                                    {QStringLiteral("lon"), item.value(QStringLiteral("lon"))},
                                    {QStringLiteral("label"), item.value(QStringLiteral("label"))}});
    }
    const QJsonObject current = stops.isEmpty() ? QJsonObject{} : stops.at(step).toObject();
    const QString latitude = current.isEmpty() ? QString() :
        QString::number(current.value(QStringLiteral("lat")).toDouble(), 'f', 6);
    const QString longitude = current.isEmpty() ? QString() :
        QString::number(current.value(QStringLiteral("lon")).toDouble(), 'f', 6);
    set(channel, QStringLiteral("waypoints"), stops.isEmpty() ? QString() :
        QString::fromUtf8(QJsonDocument(waypoints).toJson(QJsonDocument::Compact)), false);
    set(channel, QStringLiteral("current-step"), stops.isEmpty() ? QString() :
        QString::number(step), false);
    set(channel, QStringLiteral("latitude"), latitude, false);
    set(channel, QStringLiteral("longitude"), longitude, false);
    set(channel, QStringLiteral("destination"), stops.isEmpty() ? QString() :
        latitude + QLatin1Char(',') + longitude, false);
    set(channel, QStringLiteral("address"), current.value(QStringLiteral("label")).toString(), false);
    set(channel, QStringLiteral("timestamp"), stops.isEmpty() ? QString() :
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate), false);
    set(channel, QStringLiteral("revision"),
        QString::number(m_routePlan.value(QStringLiteral("revision")).toInt()), false);
    set(channel, QStringLiteral("plan"),
        QString::fromUtf8(QJsonDocument(m_routePlan).toJson(QJsonDocument::Compact)), false);
    notifySubscribers(channel, QStringLiteral("updated"));
    emit fieldsUpdated(channel, m_storage.value(channel));
}

void InMemoryMdbRepository::handleRoutePlanCall(const QString &envelopeJson)
{
    const QJsonObject envelope = QJsonDocument::fromJson(envelopeJson.toUtf8()).object();
    const QString channel = envelope.value(QStringLiteral("reply_channel")).toString();
    if (channel.isEmpty()) return;
    const QJsonObject args = envelope.value(QStringLiteral("payload")).toObject();
    const QString method = envelope.value(QStringLiteral("method")).toString();
    const QJsonArray oldStops = m_routePlan.value(QStringLiteral("stops")).toArray();
    QJsonArray stops = oldStops;
    int step = m_routePlan.value(QStringLiteral("current_step")).toInt();
    QString id = m_routePlan.value(QStringLiteral("id")).toString();
    bool keep = m_routePlan.value(QStringLiteral("keep_current_stop")).toBool();
    QString error;
    bool changed = false;
    auto newId = []() { return QUuid::createUuid().toString(QUuid::WithoutBraces); };
    auto guarded = [&]() {
        return !id.isEmpty() && args.value(QStringLiteral("expected_plan_id")).toString() == id
            && step < stops.size()
            && args.value(QStringLiteral("expected_stop_id")).toString()
               == stops.at(step).toObject().value(QStringLiteral("id")).toString();
    };
    auto newStop = [&newId](const QJsonObject &item) {
        return QJsonObject{{QStringLiteral("id"), newId()},
            {QStringLiteral("lat"), item.value(QStringLiteral("lat"))},
            {QStringLiteral("lon"), item.value(QStringLiteral("lon"))},
            {QStringLiteral("label"), item.value(QStringLiteral("label")).toString()},
            {QStringLiteral("reached"), false}};
    };
    auto validStop = [](const QJsonObject &item) {
        return item.value(QStringLiteral("lat")).isDouble()
            && item.value(QStringLiteral("lon")).isDouble()
            && qAbs(item.value(QStringLiteral("lat")).toDouble()) <= 90
            && qAbs(item.value(QStringLiteral("lon")).toDouble()) <= 180;
    };
    if (method == QLatin1String("plan.get")) {
    } else if (method == QLatin1String("plan.replace")) {
        const QJsonArray incoming = args.value(QStringLiteral("stops")).toArray();
        const int start = args.value(QStringLiteral("start_step")).toInt();
        if (incoming.isEmpty() || incoming.size() > 32 || start < 0 || start >= incoming.size())
            error = QStringLiteral("invalid stops or start_step");
        else {
            QJsonArray next;
            for (int i = 0; i < incoming.size(); ++i) {
                const QJsonObject entry = incoming.at(i).toObject();
                if (!validStop(entry)) { error = QStringLiteral("invalid stop"); break; }
                QJsonObject stop = newStop(entry);
                stop.insert(QStringLiteral("reached"), i < start);
                next.append(stop);
            }
            if (error.isEmpty()) { stops = next; step = start; id = newId(); keep = false; changed = true; }
        }
    } else if (method == QLatin1String("plan.append")) {
        const QJsonObject entry = args.value(QStringLiteral("stop")).toObject();
        if (!validStop(entry) || stops.size() >= 32) error = QStringLiteral("invalid stop");
        else {
            if (id.isEmpty()) id = newId();
            stops.append(newStop(entry)); changed = true;
        }
    } else if (method == QLatin1String("plan.remove") || method == QLatin1String("plan.move")
               || method == QLatin1String("plan.jump")) {
        if (args.value(QStringLiteral("expected_revision")).toInt(-1)
            != m_routePlan.value(QStringLiteral("revision")).toInt()) error = QStringLiteral("stale revision");
        else if (method == QLatin1String("plan.remove")) {
            const int index = args.value(QStringLiteral("index")).toInt(-1);
            if (index < 0 || index >= stops.size()) error = QStringLiteral("invalid index");
            else {
                stops.removeAt(index);
                if (stops.isEmpty()) { step = 0; id.clear(); keep = false; }
                else if (index < step || step >= stops.size()) --step;
                changed = true;
            }
        } else if (method == QLatin1String("plan.jump")) {
            const int index = args.value(QStringLiteral("index")).toInt(-1);
            if (index < 0 || index >= stops.size()) error = QStringLiteral("invalid index");
            else {
                step = index;
                keep = false;
                for (int i = 0; i < stops.size(); ++i) {
                    QJsonObject stop = stops.at(i).toObject();
                    stop.insert(QStringLiteral("reached"), i < index);
                    stops.replace(i, stop);
                }
                changed = true;
            }
        } else {
            const int from = args.value(QStringLiteral("from_index")).toInt(-1);
            const int to = args.value(QStringLiteral("to_index")).toInt(-1);
            if (from < 0 || from >= stops.size() || to < 0 || to >= stops.size())
                error = QStringLiteral("invalid index");
            else {
                const QString current = stops.at(step).toObject().value(QStringLiteral("id")).toString();
                const QJsonValue moved = stops.takeAt(from);
                stops.insert(to, moved);
                for (int i = 0; i < stops.size(); ++i)
                    if (stops.at(i).toObject().value(QStringLiteral("id")).toString() == current)
                        step = i;
                changed = true;
            }
        }
    } else if (method == QLatin1String("plan.reached") || method == QLatin1String("plan.advance")
               || method == QLatin1String("plan.unreach")
               || method == QLatin1String("plan.set-keep-current")) {
        if (!guarded()) error = QStringLiteral("stale plan or stop");
        else if (method == QLatin1String("plan.advance")) {
            if (step + 1 >= stops.size()) error = QStringLiteral("no next stop");
            else { ++step; keep = false; changed = true; }
        } else if (method == QLatin1String("plan.set-keep-current")) {
            if (args.value(QStringLiteral("keep")).toBool()) error = QStringLiteral("invalid keep");
            else if (keep) { keep = false; changed = true; }
        } else {
            const bool reached = method == QLatin1String("plan.reached");
            if (keep != !reached) { keep = !reached; changed = true; }
            QJsonObject current = stops.at(step).toObject();
            if (current.value(QStringLiteral("reached")).toBool() != reached) {
                current.insert(QStringLiteral("reached"), reached);
                stops.replace(step, current);
                changed = true;
            }
        }
    } else if (method == QLatin1String("plan.clear")) {
        const QString expected = args.value(QStringLiteral("expected_plan_id")).toString();
        if (!expected.isEmpty() && expected != id) error = QStringLiteral("stale plan");
        else if (!stops.isEmpty()) { stops = {}; step = 0; id.clear(); keep = false; changed = true; }
    } else error = QStringLiteral("unknown method");

    if (error.isEmpty() && changed) {
        m_routePlan.insert(QStringLiteral("revision"),
                           m_routePlan.value(QStringLiteral("revision")).toInt() + 1);
        m_routePlan.insert(QStringLiteral("stops"), stops);
        m_routePlan.insert(QStringLiteral("current_step"), step);
        m_routePlan.insert(QStringLiteral("id"), id);
        m_routePlan.insert(QStringLiteral("keep_current_stop"), keep);
        publishRoutePlan();
    }
    QJsonObject reply{{QStringLiteral("ok"), error.isEmpty()}};
    if (error.isEmpty()) reply.insert(QStringLiteral("payload"), m_routePlan);
    else reply.insert(QStringLiteral("error"), error);
    publish(channel, QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
}

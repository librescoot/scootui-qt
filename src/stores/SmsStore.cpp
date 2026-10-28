#include "SmsStore.h"
#include "repositories/MdbRepository.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

SmsStore::SmsStore(MdbRepository *repo, QObject *parent) : SyncableStore(repo, parent)
{
    connect(repo, &MdbRepository::connectionStateChanged, this, [this]() { emit messagesChanged(); });
}

bool SmsStore::connected() const { return m_repo && m_repo->isConnected(); }

SyncSettings SmsStore::syncSettings() const
{
    return {QStringLiteral("sms:inbox"), 30000,
            {{QStringLiteral("snapshot"), QStringLiteral("snapshot"), true}}, {}, {}};
}

void SmsStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable != QLatin1String("snapshot")) return;
    const auto doc = QJsonDocument::fromJson(value.toUtf8());
    const auto obj = doc.object();
    const bool valid = doc.isObject() && obj.value(QStringLiteral("version")).toInt() == 1
                       && obj.value(QStringLiteral("messages")).isArray();
    QVariantList messages;
    int unread = 0;
    QString forwardTo;
    if (valid) {
        messages = obj.value(QStringLiteral("messages")).toArray().toVariantList();
        unread = obj.value(QStringLiteral("unread")).toInt();
        forwardTo = obj.value(QStringLiteral("forward-to")).toString();
    }
    if (m_available == valid && m_messages == messages && m_unread == unread && m_forwardTo == forwardTo) return;
    m_available = valid;
    m_messages = messages;
    m_unread = unread;
    m_forwardTo = forwardTo;
    emit messagesChanged();
}

void SmsStore::sendCommand(const QString &action, const QString &id)
{
    static const QRegularExpression validId(QStringLiteral("^[a-f0-9]{32}$"));
    if (!connected() || !validId.match(id).hasMatch()) return;
    const QJsonObject command{{QStringLiteral("action"), action}, {QStringLiteral("id"), id}};
    m_repo->push(QStringLiteral("sms:inbox:commands"),
                 QString::fromUtf8(QJsonDocument(command).toJson(QJsonDocument::Compact)));
}

void SmsStore::markRead(const QString &id) { sendCommand(QStringLiteral("read"), id); }
void SmsStore::deleteMessage(const QString &id) { sendCommand(QStringLiteral("delete"), id); }
void SmsStore::disableForwarding() { if (connected()) m_repo->set(QStringLiteral("settings"), QStringLiteral("sms.forward-to"), QStringLiteral("off")); }

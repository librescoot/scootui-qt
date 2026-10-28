#pragma once

#include "SyncableStore.h"
#include <QVariantList>

class SmsStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(int unread READ unread NOTIFY messagesChanged)
    Q_PROPERTY(bool available READ available NOTIFY messagesChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY messagesChanged)
    Q_PROPERTY(QString forwardTo READ forwardTo NOTIFY messagesChanged)
public:
    explicit SmsStore(MdbRepository *repo, QObject *parent = nullptr);
    QVariantList messages() const { return m_messages; }
    int unread() const { return m_unread; }
    bool available() const { return m_available; }
    bool connected() const;
    QString forwardTo() const { return m_forwardTo; }
    Q_INVOKABLE void markRead(const QString &id);
    Q_INVOKABLE void deleteMessage(const QString &id);
    Q_INVOKABLE void disableForwarding();
signals:
    void messagesChanged();
protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;
private:
    void sendCommand(const QString &action, const QString &id);
    QVariantList m_messages;
    int m_unread = 0;
    bool m_available = false;
    QString m_forwardTo;
};

#include "KeycardStore.h"

KeycardStore::KeycardStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    if (m_repo) {
        m_eventSubscriptionId = m_repo->subscribe(
            QStringLiteral("keycard:events"),
            [this](const QString &, const QString &message) {
                onKeycardEvent(message);
            });
    }
}

KeycardStore::~KeycardStore()
{
    if (m_repo && m_eventSubscriptionId != 0)
        m_repo->unsubscribe(QStringLiteral("keycard:events"), m_eventSubscriptionId);
}

SyncSettings KeycardStore::syncSettings() const
{
    return {QStringLiteral("system"), 5000,
            {{QStringLiteral("keycard-learn-state"), QStringLiteral("keycard-learn-state"), true},
             {QStringLiteral("keycard-last-used-uid"), QStringLiteral("keycard-last-used-uid"), true}},
            {{QStringLiteral("keycard:authorized"), QStringLiteral("keycard:authorized"), 5000},
             {QStringLiteral("keycard:masters"), QStringLiteral("keycard:masters"), 5000},
             {QStringLiteral("keycard:phones"), QStringLiteral("keycard:phones"), 5000},
             {QStringLiteral("keycard:aliases"), QStringLiteral("keycard:aliases"), 5000}}, {}};
}

void KeycardStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("keycard-last-used-uid")) {
        if (value == m_lastUsedUid) return;
        m_lastUsedUid = value;
        emit lastUsedUidChanged();
        return;
    }
    if (variable == QLatin1String("keycard-learn-state")) {
        const QString state = value.isEmpty() ? QStringLiteral("idle") : value;
        if (state == m_learnState) return;
        m_learnState = state;
        emit learnStateChanged();
    }
}

void KeycardStore::applySetUpdate(const QString &name, const QStringList &members)
{
    if (name == QLatin1String("keycard:aliases")) {
        QHash<QString, QString> aliases;
        for (const QString &entry : members) {
            const qsizetype first = entry.indexOf(QLatin1Char(':'));
            const qsizetype second = entry.indexOf(QLatin1Char(':'), first + 1);
            if (first < 0 || second < 0) continue;
            const QString kind = entry.left(first);
            const QString id = entry.mid(first + 1, second - first - 1);
            const QString alias = entry.mid(second + 1);
            if ((kind != QLatin1String("card") && kind != QLatin1String("phone"))
                || id.isEmpty() || alias.isEmpty() || alias != alias.trimmed()
                || alias.toUtf8().size() > 32) continue;
            bool printable = true;
            for (const QChar c : alias) {
                if (!c.isPrint()) { printable = false; break; }
            }
            if (printable) aliases.insert(kind + QLatin1Char(':') + id, alias);
        }
        if (aliases != m_aliases) {
            m_aliases = aliases;
            emit aliasesChanged();
        }
        return;
    }
    QStringList sorted = members;
    sorted.sort();
    if (name == QLatin1String("keycard:authorized") && sorted != m_unlockCards) {
        m_unlockCards = sorted;
        emit unlockCardsChanged();
    } else if (name == QLatin1String("keycard:masters") && sorted != m_masterCards) {
        m_masterCards = sorted;
        emit masterCardsChanged();
    } else if (name == QLatin1String("keycard:phones") && sorted != m_phoneKeys) {
        m_phoneKeys = sorted;
        emit phoneKeysChanged();
    }
}

void KeycardStore::clearEnrollmentFeedback()
{
    if (m_sessionCards.isEmpty() && m_sessionPhones.isEmpty()
        && m_lastScannedKind.isEmpty() && m_lastScannedUid.isEmpty() && m_scanStatus.isEmpty())
        return;
    m_sessionCards.clear();
    m_sessionPhones.clear();
    m_lastScannedKind.clear();
    m_lastScannedUid.clear();
    m_scanStatus.clear();
    emit enrollmentFeedbackChanged();
}

void KeycardStore::setEnrollmentFeedback(const QString &kind, const QString &uid, const QString &status)
{
    m_lastScannedKind = kind;
    m_lastScannedUid = uid;
    m_scanStatus = status;
    emit enrollmentFeedbackChanged();
}

void KeycardStore::onKeycardEvent(const QString &message)
{
    const QStringList parts = message.split(':');
    if (parts.isEmpty()) return;

    const QString &event = parts[0];
    if (event == QLatin1String("mode-entered")) {
        clearEnrollmentFeedback();
        return;
    }
    if (event == QLatin1String("reset")) {
        clearEnrollmentFeedback();
        return;
    }
    if (event == QLatin1String("phone-rejected") || event == QLatin1String("phone-error")) {
        setEnrollmentFeedback(QStringLiteral("phone"), {},
                              event == QLatin1String("phone-error") ? QStringLiteral("error")
                                                                  : QStringLiteral("rejected"));
        return;
    }
    if (parts.size() < 2) return;

    const QString &uid = parts.last();
    if (event == QLatin1String("card-learned")) {
        if (!m_sessionCards.contains(uid)) m_sessionCards.append(uid);
        setEnrollmentFeedback(QStringLiteral("card"), uid, QStringLiteral("accepted"));
    } else if (event == QLatin1String("phone-learned")) {
        if (!m_sessionPhones.contains(uid)) m_sessionPhones.append(uid);
        setEnrollmentFeedback(QStringLiteral("phone"), uid, QStringLiteral("accepted"));
    } else if (event == QLatin1String("card-duplicate") || event == QLatin1String("phone-duplicate")) {
        setEnrollmentFeedback(event == QLatin1String("phone-duplicate") ? QStringLiteral("phone")
                                                                        : QStringLiteral("card"),
                              uid, QStringLiteral("duplicate"));
    } else if (event == QLatin1String("rejected")) {
        setEnrollmentFeedback(QStringLiteral("card"), uid, QStringLiteral("rejected"));
    } else if (event == QLatin1String("error")) {
        setEnrollmentFeedback(QStringLiteral("card"), uid, QStringLiteral("error"));
    } else if (event == QLatin1String("master-learned")
               || (event == QLatin1String("master-added") && parts.size() >= 3)) {
        setEnrollmentFeedback(QStringLiteral("card"), parts[1], QStringLiteral("accepted"));
    }
}

void KeycardStore::startEnroll()
{
    clearEnrollmentFeedback();
    if (m_repo) m_repo->push("scooter:keycard", "learn:start");
}
void KeycardStore::stopEnroll() { if (m_repo) m_repo->push("scooter:keycard", "learn:stop"); }
void KeycardStore::startMasterEnroll()
{
    clearEnrollmentFeedback();
    if (m_repo) m_repo->push("scooter:keycard", "learn:master:start");
}
void KeycardStore::stopMasterEnroll() { if (m_repo) m_repo->push("scooter:keycard", "learn:master:stop"); }
void KeycardStore::skipMasterBootstrap()
{
    if (!m_repo) return;
    m_repo->push("scooter:keycard", "set-master:NONE");
    m_repo->push("scooter:keycard", "learn:start");
}
void KeycardStore::removeCard(const QString &uid) { if (m_repo) m_repo->push("scooter:keycard", "remove:" + uid); }
void KeycardStore::removeCardForced(const QString &uid) { if (m_repo) m_repo->push("scooter:keycard", "remove:" + uid + ":force"); }
void KeycardStore::removeMaster(const QString &uid) { if (m_repo) m_repo->push("scooter:keycard", "master:remove:" + uid); }
void KeycardStore::removePhone(const QString &fingerprint) { if (m_repo) m_repo->push("scooter:keycard", "phone:remove:" + fingerprint); }
void KeycardStore::removePhoneForced(const QString &fingerprint) { if (m_repo) m_repo->push("scooter:keycard", "phone:remove:" + fingerprint + ":force"); }

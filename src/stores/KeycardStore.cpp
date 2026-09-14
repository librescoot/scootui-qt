#include "KeycardStore.h"

KeycardStore::KeycardStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings KeycardStore::syncSettings() const
{
    return {QStringLiteral("system"), 5000,
            {{QStringLiteral("keycard-learn-state"), QStringLiteral("keycard-learn-state"), true}},
            {{QStringLiteral("keycard:authorized"), QStringLiteral("keycard:authorized"), 5000},
             {QStringLiteral("keycard:masters"), QStringLiteral("keycard:masters"), 5000}}, {}};
}

void KeycardStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable != QLatin1String("keycard-learn-state")) return;
    const QString state = value.isEmpty() ? QStringLiteral("idle") : value;
    if (state == m_learnState) return;
    m_learnState = state;
    emit learnStateChanged();
}

void KeycardStore::applySetUpdate(const QString &name, const QStringList &members)
{
    QStringList sorted = members;
    sorted.sort();
    if (name == QLatin1String("keycard:authorized") && sorted != m_unlockCards) {
        m_unlockCards = sorted;
        emit unlockCardsChanged();
    } else if (name == QLatin1String("keycard:masters") && sorted != m_masterCards) {
        m_masterCards = sorted;
        emit masterCardsChanged();
    }
}

void KeycardStore::startEnroll() { if (m_repo) m_repo->push("scooter:keycard", "learn:start"); }
void KeycardStore::stopEnroll() { if (m_repo) m_repo->push("scooter:keycard", "learn:stop"); }
void KeycardStore::startMasterEnroll() { if (m_repo) m_repo->push("scooter:keycard", "learn:master:start"); }
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

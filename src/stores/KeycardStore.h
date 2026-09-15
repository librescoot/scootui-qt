#pragma once

#include <QStringList>

#include "SyncableStore.h"

// Mirrors the durable card snapshots and enrollment mode published by
// keycard-service for the dashboard's card-management screens.
class KeycardStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(bool learning READ learning NOTIFY learnStateChanged)
    Q_PROPERTY(bool masterTeachIn READ masterTeachIn NOTIFY learnStateChanged)
    Q_PROPERTY(bool masterBootstrap READ masterBootstrap NOTIFY learnStateChanged)
    Q_PROPERTY(bool enrollActive READ enrollActive NOTIFY learnStateChanged)
    Q_PROPERTY(QStringList unlockCards READ unlockCards NOTIFY unlockCardsChanged)
    Q_PROPERTY(QStringList masterCards READ masterCards NOTIFY masterCardsChanged)
    Q_PROPERTY(int unlockCardCount READ unlockCardCount NOTIFY unlockCardsChanged)
    Q_PROPERTY(int sessionCardCount READ sessionCardCount NOTIFY enrollmentFeedbackChanged)
    Q_PROPERTY(QString lastScannedUid READ lastScannedUid NOTIFY enrollmentFeedbackChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY enrollmentFeedbackChanged)

public:
    explicit KeycardStore(MdbRepository *repo, QObject *parent = nullptr);
    ~KeycardStore() override;

    bool learning() const { return m_learnState == QLatin1String("learn"); }
    bool masterTeachIn() const { return m_learnState == QLatin1String("master-teach-in"); }
    bool masterBootstrap() const { return m_learnState == QLatin1String("master-bootstrap"); }
    bool enrollActive() const { return learning() || masterTeachIn() || masterBootstrap(); }
    QStringList unlockCards() const { return m_unlockCards; }
    QStringList masterCards() const { return m_masterCards; }
    int unlockCardCount() const { return m_unlockCards.size(); }
    int sessionCardCount() const { return m_sessionCards.size(); }
    QString lastScannedUid() const { return m_lastScannedUid; }
    QString scanStatus() const { return m_scanStatus; }

    Q_INVOKABLE void startEnroll();
    Q_INVOKABLE void stopEnroll();
    Q_INVOKABLE void startMasterEnroll();
    Q_INVOKABLE void stopMasterEnroll();
    Q_INVOKABLE void skipMasterBootstrap();
    Q_INVOKABLE void removeCard(const QString &uid);
    Q_INVOKABLE void removeCardForced(const QString &uid);
    Q_INVOKABLE void removeMaster(const QString &uid);

signals:
    void learnStateChanged();
    void unlockCardsChanged();
    void masterCardsChanged();
    void enrollmentFeedbackChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;
    void applySetUpdate(const QString &name, const QStringList &members) override;

private:
    void onKeycardEvent(const QString &message);
    void clearEnrollmentFeedback();
    void setEnrollmentFeedback(const QString &uid, const QString &status);

    QString m_learnState = QStringLiteral("idle");
    QStringList m_unlockCards;
    QStringList m_masterCards;
    QStringList m_sessionCards;
    QString m_lastScannedUid;
    QString m_scanStatus;
    SubscriptionId m_eventSubscriptionId = 0;
};

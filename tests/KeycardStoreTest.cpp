#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "stores/KeycardStore.h"

class RecordingRepository : public InMemoryMdbRepository
{
public:
    void push(const QString &channel, const QString &command) override
    {
        pushed << qMakePair(channel, command);
    }
    QList<QPair<QString, QString>> pushed;
};

class DelayedSetRepository : public InMemoryMdbRepository
{
public:
    void requestSetMembers(const QString &setKey) override
    {
        requestedSets.append(setKey);
    }

    void deliver(const QString &setKey)
    {
        emit setMembersFetched(setKey, getSetMembers(setKey));
    }

    QStringList requestedSets;
};

class KeycardStoreTest : public QObject
{
    Q_OBJECT
private slots:
    void hydratesSortedSnapshotsAndMode();
    void refreshesSetOnSystemNotification();
    void appliesDelayedSetResult();
    void tracksEnrollmentFeedback();
    void emitsCurrentCommandVocabulary();
};

void KeycardStoreTest::hydratesSortedSnapshotsAndMode()
{
    RecordingRepository repo;
    repo.addToSet("keycard:authorized", "B");
    repo.addToSet("keycard:authorized", "A");
    repo.addToSet("keycard:masters", "D");
    repo.addToSet("keycard:phones", "F7C6A7D0309ED723119742DE3F197846");
    repo.set("system", "keycard-learn-state", "master-bootstrap", false);
    repo.set("system", "keycard-last-used-uid", "B", false);
    KeycardStore store(&repo);
    QSignalSpy stateChanged(&store, &KeycardStore::learnStateChanged);
    store.start();
    QCOMPARE(store.unlockCards(), QStringList({"A", "B"}));
    QCOMPARE(store.masterCards(), QStringList({"D"}));
    QCOMPARE(store.phoneKeys(), QStringList({"F7C6A7D0309ED723119742DE3F197846"}));
    QCOMPARE(store.lastUsedUid(), QStringLiteral("B"));
    QVERIFY(store.masterBootstrap());
    QVERIFY(store.enrollActive());
    repo.set("system", "keycard-learn-state", "learn");
    QVERIFY(store.learning());
    QVERIFY(!store.masterBootstrap());
    QVERIFY(stateChanged.count() >= 1);
}

void KeycardStoreTest::refreshesSetOnSystemNotification()
{
    RecordingRepository repo;
    KeycardStore store(&repo);
    store.start();
    QSignalSpy changed(&store, &KeycardStore::unlockCardsChanged);
    QSignalSpy phonesChanged(&store, &KeycardStore::phoneKeysChanged);
    QSignalSpy lastUsedChanged(&store, &KeycardStore::lastUsedUidChanged);
    repo.addToSet("keycard:authorized", "CAFE");
    repo.publish("system", "keycard:authorized");
    QCOMPARE(store.unlockCards(), QStringList({"CAFE"}));
    QCOMPARE(changed.count(), 1);
    repo.addToSet("keycard:phones", "F7C6A7D0309ED723119742DE3F197846");
    repo.publish("system", "keycard:phones");
    QCOMPARE(store.phoneKeys(), QStringList({"F7C6A7D0309ED723119742DE3F197846"}));
    QCOMPARE(phonesChanged.count(), 1);
    repo.set("system", "keycard-last-used-uid", "CAFE");
    QCOMPARE(store.lastUsedUid(), QStringLiteral("CAFE"));
    QCOMPARE(lastUsedChanged.count(), 1);
}

void KeycardStoreTest::appliesDelayedSetResult()
{
    DelayedSetRepository repo;
    repo.addToSet("keycard:authorized", "CAFE");
    KeycardStore store(&repo);
    store.start();

    QVERIFY(store.unlockCards().isEmpty());
    QVERIFY(repo.requestedSets.contains("keycard:authorized"));
    repo.deliver("keycard:authorized");
    QCOMPARE(store.unlockCards(), QStringList({"CAFE"}));
}

void KeycardStoreTest::tracksEnrollmentFeedback()
{
    RecordingRepository repo;
    KeycardStore store(&repo);
    store.start();
    QSignalSpy changed(&store, &KeycardStore::enrollmentFeedbackChanged);

    repo.publish("keycard:events", "mode-entered:learn:command");
    repo.publish("keycard:events", "card-learned:CAFE");
    QCOMPARE(store.sessionCardCount(), 1);
    QCOMPARE(store.lastScannedUid(), QStringLiteral("CAFE"));
    QCOMPARE(store.scanStatus(), QStringLiteral("accepted"));

    repo.publish("keycard:events", "card-duplicate:CAFE");
    QCOMPARE(store.sessionCardCount(), 1);
    QCOMPARE(store.scanStatus(), QStringLiteral("duplicate"));

    repo.publish("keycard:events", "rejected:already-authorized:BEEF");
    QCOMPARE(store.lastScannedUid(), QStringLiteral("BEEF"));
    QCOMPARE(store.scanStatus(), QStringLiteral("rejected"));

    repo.publish("keycard:events", "mode-entered:master");
    QCOMPARE(store.sessionCardCount(), 0);
    QVERIFY(store.lastScannedUid().isEmpty());
    QVERIFY(store.scanStatus().isEmpty());
    QVERIFY(changed.count() >= 4);
}

void KeycardStoreTest::emitsCurrentCommandVocabulary()
{
    RecordingRepository repo;
    KeycardStore store(&repo);
    store.start();
    store.startEnroll();
    store.stopEnroll();
    store.startMasterEnroll();
    store.stopMasterEnroll();
    store.skipMasterBootstrap();
    store.removeCard("AA");
    store.removeCardForced("BB");
    store.removeMaster("CC");
    store.removePhone("F7C6A7D0309ED723119742DE3F197846");
    store.removePhoneForced("D7C6A7D0309ED723119742DE3F197846");
    const QList<QPair<QString, QString>> expected = {
        {"scooter:keycard", "learn:start"}, {"scooter:keycard", "learn:stop"},
        {"scooter:keycard", "learn:master:start"}, {"scooter:keycard", "learn:master:stop"},
        {"scooter:keycard", "set-master:NONE"}, {"scooter:keycard", "learn:start"},
        {"scooter:keycard", "remove:AA"}, {"scooter:keycard", "remove:BB:force"},
        {"scooter:keycard", "master:remove:CC"},
        {"scooter:keycard", "phone:remove:F7C6A7D0309ED723119742DE3F197846"},
        {"scooter:keycard", "phone:remove:D7C6A7D0309ED723119742DE3F197846:force"}
    };
    QCOMPARE(repo.pushed, expected);
}

QTEST_GUILESS_MAIN(KeycardStoreTest)
#include "KeycardStoreTest.moc"

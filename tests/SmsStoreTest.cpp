#include <QtTest>
#include "repositories/InMemoryMdbRepository.h"
#include "stores/SmsStore.h"

class SmsRepository : public InMemoryMdbRepository
{
public:
    QString queue;
    QString payload;
    void push(const QString &channel, const QString &command) override { queue = channel; payload = command; }
};

class SmsStoreTest : public QObject
{
    Q_OBJECT
private slots:
    void snapshotAndCommands();
};

void SmsStoreTest::snapshotAndCommands()
{
    SmsRepository repo;
    SmsStore store(&repo);
    store.start();
    repo.set(QStringLiteral("sms:inbox"), QStringLiteral("snapshot"),
             QStringLiteral(R"({"version":1,"unread":1,"forward-to":"+491234567890","messages":[{"id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","text":"hello","read":false}]})"));
    QVERIFY(store.available());
    QCOMPARE(store.unread(), 1);
    QCOMPARE(store.messages().size(), 1);
    QCOMPARE(store.forwardTo(), QStringLiteral("+491234567890"));
    store.markRead(QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    QCOMPARE(repo.queue, QStringLiteral("sms:inbox:commands"));
    QVERIFY(repo.payload.contains(QStringLiteral("\"action\":\"read\"")));
    repo.payload.clear();
    store.deleteMessage(QStringLiteral("invalid"));
    QVERIFY(repo.payload.isEmpty());
    store.disableForwarding();
    QCOMPARE(repo.get(QStringLiteral("settings"), QStringLiteral("sms.forward-to")), QStringLiteral("off"));
    repo.set(QStringLiteral("sms:inbox"), QStringLiteral("snapshot"), QStringLiteral("bad"));
    QVERIFY(!store.available());
    QVERIFY(store.messages().isEmpty());
}

QTEST_MAIN(SmsStoreTest)
#include "SmsStoreTest.moc"

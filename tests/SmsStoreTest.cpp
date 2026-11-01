#include <QtTest>
#include "repositories/InMemoryMdbRepository.h"
#include "stores/SmsStore.h"

class SmsRepository : public InMemoryMdbRepository
{
public:
    QString queue;
    QString payload;
    bool online = true;
    bool isConnected() const override { return online; }
    void push(const QString &channel, const QString &command) override { queue = channel; payload = command; }
};

class SmsStoreTest : public QObject
{
    Q_OBJECT
private slots:
    void snapshotAndCommands();
    void openByIdentityAndReadOffline();
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

void SmsStoreTest::openByIdentityAndReadOffline()
{
    SmsRepository repo;
    SmsStore store(&repo);
    store.start();
    const QString id(32, QLatin1Char('a'));
    repo.set("sms:inbox", "snapshot", QStringLiteral(R"({"version":1,"messages":[{"id":"%1","text":"hello"}]})").arg(id));
    QSignalSpy opened(&store, &SmsStore::openedMessageChanged);
    QVERIFY(store.openMessage(id));
    QCOMPARE(opened.size(), 1);
    QCOMPARE(store.openedMessage().value("text").toString(), QStringLiteral("hello"));
    QVERIFY(repo.payload.contains("\"action\":\"read\""));
    repo.payload.clear();
    QVERIFY(!store.openMessage(QString(32, QLatin1Char('b'))));
    QCOMPARE(opened.size(), 1);
    QVERIFY(repo.payload.isEmpty());
    repo.online = false;
    emit repo.connectionStateChanged(false);
    QVERIFY(store.openMessage(id));
    QCOMPARE(opened.size(), 2);
    store.deleteMessage(id);
    store.disableForwarding();
    QVERIFY(repo.payload.isEmpty());
    QVERIFY(repo.get("settings", "sms.forward-to").isEmpty());
    repo.set("sms:inbox", "snapshot", QStringLiteral(R"({"version":1,"messages":[]})"));
    QVERIFY(!store.openMessage(id));
    QCOMPARE(store.openedMessage().value("text").toString(), QStringLiteral("hello"));
}

QTEST_MAIN(SmsStoreTest)
#include "SmsStoreTest.moc"

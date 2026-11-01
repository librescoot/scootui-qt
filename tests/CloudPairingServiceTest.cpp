#include <QtTest>
#include <QJsonDocument>
#include "services/CloudPairingService.h"
#include "repositories/InMemoryMdbRepository.h"

class PairingRepository : public InMemoryMdbRepository {
    Q_OBJECT
public:
    bool connected = true, acknowledged = false, autoReply = true;
    QStringList methods;
    QJsonObject snapshot{{"state", "not_registered"}, {"running", false}};
    QJsonObject last;
    explicit PairingRepository(QObject *parent = nullptr) : InMemoryMdbRepository(parent) {
        connect(this, &MdbRepository::subscriptionReady, this, [this](const QString &) { acknowledged = true; });
    }
    bool isConnected() const override { return connected; }
    SubscriptionId subscribe(const QString &channel, SubscriptionCallback callback) override {
        acknowledged = false;
        return InMemoryMdbRepository::subscribe(channel, std::move(callback));
    }
    void push(const QString &channel, const QString &data) override {
        QVERIFY(acknowledged);
        QCOMPARE(channel, QStringLiteral("cloud:pairing"));
        last = QJsonDocument::fromJson(data.toUtf8()).object();
        methods << last.value("method").toString();
        QCOMPARE(last.value("reply_channel").toString(), channel + ":reply:" + last.value("id").toString());
        QVERIFY(last.value("deadline").toDouble() > QDateTime::currentMSecsSinceEpoch());
        QVERIFY(last.value("payload").toObject().isEmpty());
        if (autoReply) reply(last.value("reply_channel").toString(), snapshot);
    }
    void reply(const QString &channel, const QJsonObject &value) {
        const auto response = QJsonDocument(QJsonObject{{"ok", true}, {"payload", value}}).toJson(QJsonDocument::Compact);
        publish(channel, QString::fromUtf8(response));
    }
};

class CloudPairingServiceTest : public QObject {
    Q_OBJECT
private slots:
    void unknownInternetDoesNotExposeCodeOrStart() {
        PairingRepository repo;
        repo.snapshot = {{"state", "waiting_for_web_setup"}, {"running", true},
            {"code", "01234-56789"}, {"expires_at", QDateTime::currentSecsSinceEpoch() + 300}};
        CloudPairingService service(&repo);
        service.setStationary(true);
        service.setVisible(true);
        QTRY_COMPARE(service.state(), QStringLiteral("waiting_for_web_setup"));
        QVERIFY(!service.online());
        QVERIFY(!service.canStart());
        QVERIFY(!service.canStop());
        QVERIFY(service.code().isEmpty());
        service.setInternetAvailable(true);
        QTRY_COMPARE(service.code(), QStringLiteral("01234-56789"));
    }
    void explicitStartAndStationaryGuard() {
        PairingRepository repo;
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        QCOMPARE(repo.methods.size(), 0);
        service.setVisible(true);
        QTRY_COMPARE(service.state(), QStringLiteral("not_registered"));
        QCOMPARE(repo.methods, QStringList{"status"});
        QVERIFY(!service.canStart());
        service.start();
        QCOMPARE(repo.methods.size(), 1);
        service.setStationary(true);
        QVERIFY(service.canStart());
        repo.snapshot = {{"state", "starting"}, {"running", true}};
        service.start();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(repo.methods.last(), QStringLiteral("start"));
        QVERIFY(!service.canStart());
        QVERIFY(service.canStop());
        repo.snapshot = {{"state", "paused"}, {"running", false}};
        service.stop();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(repo.methods.last(), QStringLiteral("stop"));
        QVERIFY(service.canStart());
    }
    void internetLossDoesNotInterruptMutation() {
        PairingRepository repo;
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        service.setStationary(true);
        service.setVisible(true);
        QTRY_VERIFY(service.canStart());
        repo.autoReply = false;
        service.start();
        QTRY_COMPARE(repo.methods.last(), QStringLiteral("start"));
        const QString channel = repo.last.value("reply_channel").toString();
        service.setInternetAvailable(false);
        QVERIFY(!service.online());
        QVERIFY(!service.canStart());
        QVERIFY(!service.canStop());
        repo.reply(channel, {{"state", "starting"}, {"running", true}});
        QVERIFY(!service.busy());
        service.setInternetAvailable(true);
        QVERIFY(service.online());
        QVERIFY(service.canStop());
    }
    void expiryPrivacyAndDisconnect() {
        PairingRepository repo;
        repo.snapshot = {{"state", "waiting_for_web_setup"}, {"running", true},
            {"code", "01234-56789"}, {"expires_at", QDateTime::currentSecsSinceEpoch() + 1},
            {"url", "https://attacker.invalid"}, {"token", "private-token"}};
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        service.setVisible(true);
        QTRY_COMPARE(service.code(), QStringLiteral("01234-56789"));
        QTRY_VERIFY_WITH_TIMEOUT(service.code().isEmpty(), 2500);
        QTRY_VERIFY_WITH_TIMEOUT(service.expired(), 2500);
        repo.snapshot["expires_at"] = QDateTime::currentSecsSinceEpoch() + 300;
        service.refresh();
        QTRY_COMPARE(service.code(), QStringLiteral("01234-56789"));
        service.setVisible(false);
        QVERIFY(service.code().isEmpty());
        QVERIFY(!service.canStart());
        const auto count = repo.methods.size();
        service.refresh();
        QCOMPARE(repo.methods.size(), count);
        repo.connected = false;
        emit repo.connectionStateChanged(false);
        QCOMPARE(service.state(), QStringLiteral("unavailable"));
        QVERIFY(!service.canStart());
    }
    void reopenWaitsForFreshStatus() {
        PairingRepository repo;
        repo.snapshot = {{"state", "waiting_for_web_setup"}, {"running", true},
            {"code", "01234-56789"}, {"expires_at", QDateTime::currentSecsSinceEpoch() + 300}};
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        service.setVisible(true);
        QTRY_COMPARE(service.code(), QStringLiteral("01234-56789"));
        service.setVisible(false);
        repo.autoReply = false;
        service.setVisible(true);
        QVERIFY(service.code().isEmpty());
        QTRY_COMPARE(repo.methods.size(), 2);
        QVERIFY(service.code().isEmpty());
        repo.reply(repo.last.value("reply_channel").toString(), repo.snapshot);
        QTRY_COMPARE(service.code(), QStringLiteral("01234-56789"));
    }
    void pollingIsPreemptedAndMutationsStaySerialized() {
        PairingRepository repo;
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        service.setStationary(true);
        service.setVisible(true);
        QTRY_COMPARE(service.state(), QStringLiteral("not_registered"));
        repo.autoReply = false;
        service.refresh();
        QTRY_COMPARE(repo.methods.size(), 2);
        const QString staleChannel = repo.last.value("reply_channel").toString();
        QVERIFY(service.canStart());
        service.start();
        QCOMPARE(service.state(), QStringLiteral("starting"));
        QVERIFY(service.busy());
        service.start(); service.stop();
        QTRY_COMPARE(repo.methods.size(), 3);
        const QString startChannel = repo.last.value("reply_channel").toString();
        repo.reply(staleChannel, {{"state", "connected"}, {"running", false}});
        QCOMPARE(service.state(), QStringLiteral("starting"));
        service.setVisible(false);
        service.setVisible(true);
        service.start();
        QCOMPARE(repo.methods.size(), 3);
        repo.reply(startChannel, {{"state", "starting"}, {"running", true}});
        QVERIFY(!service.busy());
        QVERIFY(service.canStop());
        service.stop();
        QCOMPARE(service.state(), QStringLiteral("stopping"));
        service.stop(); service.start();
        QTRY_COMPARE(repo.methods.size(), 4);
        repo.connected = false;
        emit repo.connectionStateChanged(false);
        QCOMPARE(service.state(), QStringLiteral("unavailable"));
        QVERIFY(!service.canStart());
    }
    void failedStatusStaysActionableDuringBackgroundPolls() {
        PairingRepository repo;
        repo.autoReply = false;
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        service.setVisible(true);
        QTRY_COMPARE(repo.methods.size(), 1);
        QVERIFY(service.checkingStatus());
        repo.reply(repo.last.value("reply_channel").toString(), {{"state", "invalid"}, {"running", false}});
        QCOMPARE(service.state(), QStringLiteral("unavailable"));
        QVERIFY(!service.checkingStatus());
        service.refresh();
        QTRY_COMPARE(repo.methods.size(), 2);
        QVERIFY(service.loadingStatus());
        QVERIFY(!service.checkingStatus());
        const QString stale = repo.last.value("reply_channel").toString();
        service.retry();
        QTRY_COMPARE(repo.methods.size(), 3);
        QVERIFY(service.checkingStatus());
        repo.reply(stale, {{"state", "connected"}, {"running", false}});
        QCOMPARE(service.state(), QStringLiteral("unavailable"));
        repo.reply(repo.last.value("reply_channel").toString(), {{"state", "registered"}, {"running", false}});
        QCOMPARE(service.state(), QStringLiteral("registered"));
        QVERIFY(!service.checkingStatus());
    }
    void timedOutMutationRejectsLateReply() {
        PairingRepository repo;
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        service.setStationary(true);
        service.setVisible(true);
        QTRY_VERIFY(service.canStart());
        repo.autoReply = false;
        service.start();
        QTRY_COMPARE(repo.methods.last(), QStringLiteral("start"));
        const QString channel = repo.last.value("reply_channel").toString();
        service.setVisible(false);
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QStringLiteral("unavailable"), 12000);
        QVERIFY(!service.busy());
        repo.reply(channel, {{"state", "waiting_for_web_setup"}, {"running", true},
                             {"code", "01234-56789"}, {"expires_at", QDateTime::currentSecsSinceEpoch() + 300}});
        QCOMPARE(service.state(), QStringLiteral("unavailable"));
        QVERIFY(service.code().isEmpty());
        repo.autoReply = true;
        service.setVisible(true);
        QTRY_COMPARE(service.state(), QStringLiteral("not_registered"));
        QVERIFY(service.canStart());
    }
    void unknownStatesAndUnsafeCodesAreNotExposed() {
        PairingRepository repo;
        repo.snapshot = {{"state", "waiting_for_web_setup"}, {"running", true},
            {"code", "<unsafe>"}, {"expires_at", QDateTime::currentSecsSinceEpoch() + 300}};
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        service.setVisible(true);
        QTRY_COMPARE(service.state(), QStringLiteral("waiting_for_web_setup"));
        QVERIFY(service.code().isEmpty());
        repo.snapshot["code"] = "01234-56789\n";
        service.refresh();
        QTRY_COMPARE(repo.methods.size(), 2);
        QVERIFY(service.code().isEmpty());
        repo.snapshot["code"] = "01234-56789";
        repo.snapshot["expires_at"] = QDateTime::currentSecsSinceEpoch() - 1;
        service.refresh();
        QTRY_COMPARE(repo.methods.size(), 3);
        QVERIFY(service.code().isEmpty());
        repo.snapshot["state"] = "private-unknown-state";
        service.refresh();
        QTRY_COMPARE(service.state(), QStringLiteral("unavailable"));
    }
};
QTEST_GUILESS_MAIN(CloudPairingServiceTest)
#include "CloudPairingServiceTest.moc"

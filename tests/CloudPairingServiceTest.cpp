#include <QtTest>
#include <QJsonDocument>
#include "services/CloudPairingService.h"
#include "repositories/InMemoryMdbRepository.h"

class PairingRepository : public InMemoryMdbRepository {
    Q_OBJECT
public:
    bool connected = true, acknowledged = false;
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
        const auto reply = QJsonDocument(QJsonObject{{"ok", true}, {"payload", snapshot}}).toJson(QJsonDocument::Compact);
        publish(last.value("reply_channel").toString(), QString::fromUtf8(reply));
    }
};

class CloudPairingServiceTest : public QObject {
    Q_OBJECT
private slots:
    void explicitStartAndStationaryGuard() {
        PairingRepository repo;
        CloudPairingService service(&repo);
        QCOMPARE(repo.methods.size(), 0);
        service.setVisible(true);
        QTRY_VERIFY(!service.busy());
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
    void expiryPrivacyAndDisconnect() {
        PairingRepository repo;
        repo.snapshot = {{"state", "waiting_for_web_setup"}, {"running", true},
            {"code", "01234-56789"}, {"expires_at", QDateTime::currentSecsSinceEpoch() + 1},
            {"url", "https://attacker.invalid"}, {"token", "private-token"}};
        CloudPairingService service(&repo);
        service.setVisible(true);
        QTRY_COMPARE(service.code(), QStringLiteral("01234-56789"));
        QTRY_VERIFY_WITH_TIMEOUT(service.code().isEmpty(), 2500);
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
    void unknownStatesAndUnsafeCodesAreNotExposed() {
        PairingRepository repo;
        repo.snapshot = {{"state", "waiting_for_web_setup"}, {"running", true},
            {"code", "<unsafe>"}, {"expires_at", QDateTime::currentSecsSinceEpoch() + 300}};
        CloudPairingService service(&repo);
        service.setVisible(true);
        QTRY_VERIFY(!service.busy());
        QVERIFY(service.code().isEmpty());
        repo.snapshot["state"] = "private-unknown-state";
        service.refresh();
        QTRY_COMPARE(service.state(), QStringLiteral("unavailable"));
    }
};
QTEST_GUILESS_MAIN(CloudPairingServiceTest)
#include "CloudPairingServiceTest.moc"

#include <QtTest>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickItem>
#include <QFontDatabase>
#include <QDir>
#include <QJsonDocument>
#include "services/CloudPairingService.h"
#include "repositories/InMemoryMdbRepository.h"
#include "l10n/Translations.h"
#include "services/PairingQrItem.h"

static QString pairingScreenSource()
{
    const QString override = qEnvironmentVariable("SCOOTUI_PAIRING_SCREEN_SOURCE");
    return override.isEmpty() ? QFINDTESTDATA("../qml/screens/CloudPairingScreen.qml") : override;
}

class ScreenPairingRepository : public InMemoryMdbRepository {
    Q_OBJECT
public:
    QJsonObject snapshot{{"state", "registered"}, {"running", false}};
    int delayStatus = 0, statusRequests = 0;
    void push(const QString &channel, const QString &data) override {
        if (channel != QLatin1String("cloud:pairing")) return;
        const auto envelope = QJsonDocument::fromJson(data.toUtf8()).object();
        const auto replyChannel = envelope.value("reply_channel").toString();
        const bool start = envelope.value("method").toString() == QLatin1String("start");
        if (!start) ++statusRequests;
        const QJsonObject response = start ? QJsonObject{{"state", "starting"}, {"running", true}} : snapshot;
        QTimer::singleShot(start ? 600 : delayStatus, this, [this, replyChannel, response]() {
            publish(replyChannel, QString::fromUtf8(QJsonDocument(QJsonObject{{"ok", true}, {"payload", response}}).toJson(QJsonDocument::Compact)));
        });
    }
};

class PairingTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDark MEMBER dark CONSTANT)
    Q_PROPERTY(int fontMicro READ fontMicro CONSTANT)
    Q_PROPERTY(int fontBody READ fontBody CONSTANT)
    Q_PROPERTY(int fontCaption READ fontCaption CONSTANT)
public:
    bool dark = true;
    int fontMicro() const { return 10; }
    int fontBody() const { return 18; }
    int fontCaption() const { return 14; }
};
class PairingInput : public QObject {
    Q_OBJECT
signals:
    void leftHold();
    void rightTap();
};
class PairingScreenModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state MEMBER state NOTIFY changed)
    Q_PROPERTY(QString code MEMBER code NOTIFY changed)
    Q_PROPERTY(qint64 expiresAt MEMBER expiresAt NOTIFY changed)
    Q_PROPERTY(bool canStart MEMBER canStart NOTIFY changed)
    Q_PROPERTY(bool canStop MEMBER canStop NOTIFY changed)
    Q_PROPERTY(bool online MEMBER online NOTIFY changed)
    Q_PROPERTY(bool running MEMBER running NOTIFY changed)
    Q_PROPERTY(bool expired MEMBER expired NOTIFY changed)
    Q_PROPERTY(bool loadingStatus MEMBER loadingStatus NOTIFY changed)
    Q_PROPERTY(bool checkingStatus MEMBER checkingStatus NOTIFY changed)
public:
    QString state = "waiting_for_web_setup", code = "WWWWW-WWWWW";
    qint64 expiresAt = QDateTime::currentSecsSinceEpoch() + 900;
    bool canStart = false, canStop = true, online = true, running = true, expired = false, loadingStatus = false, checkingStatus = false;
    int starts = 0, stops = 0;
    bool simulate = false;
    Q_INVOKABLE void start() {
        ++starts;
        if (!simulate) return;
        state = "starting"; running = true; canStart = false; code.clear(); emit changed();
        QTimer::singleShot(300, this, [this]() { state = "waiting_for_web_setup"; canStop = true; emit changed(); });
        QTimer::singleShot(900, this, [this]() { code = "01234-ABCDE"; expiresAt = QDateTime::currentSecsSinceEpoch() + 900; emit changed(); });
    }
    Q_INVOKABLE void stop() { ++stops; }
    Q_INVOKABLE void retry() {}
signals:
    void changed();
};
class PairingScreenNavigation : public QObject {
    Q_OBJECT
public:
    bool closed = false, resumed = false;
    CloudPairingService *service = nullptr;
    Q_INVOKABLE void closeCloudPairing() { closed = true; if (service) service->setVisible(false); }
    Q_INVOKABLE void resume() { resumed = true; }
};
class CloudPairingScreenTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qmlRegisterType<PairingQrItem>("ScootUI.Pairing", 1, 0, "PairingQr"); }
    void serviceTimedSequence_data() {
        QTest::addColumn<QString>("language");
        QTest::addColumn<bool>("dark");
        for (const auto &language : {QString("en"), QString("de")})
            for (bool dark : {true, false})
                QTest::newRow(qPrintable(language + (dark ? "-dark" : "-light"))) << language << dark;
    }
    void serviceTimedSequence() {
        QFETCH(QString, language); QFETCH(bool, dark);
        PairingTheme theme; theme.dark = dark;
        PairingInput input; PairingScreenNavigation nav; Translations translations; translations.setLanguage(language);
        ScreenPairingRepository repo;
        repo.snapshot = {{"state", "waiting_for_web_setup"}, {"running", false},
                         {"code", "01234-ABCDE"}, {"expires_at", QDateTime::currentSecsSinceEpoch() + 900}};
        repo.delayStatus = 800;
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        service.setStationary(true);
        nav.service = &service;
        QQuickView view;
        view.resize(480, 480);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.rootContext()->setContextProperty("themeStore", &theme);
        view.rootContext()->setContextProperty("inputHandler", &input);
        view.rootContext()->setContextProperty("cloudPairingService", &service);
        view.rootContext()->setContextProperty("screenStore", &nav);
        view.rootContext()->setContextProperty("menuStore", &nav);
        view.rootContext()->setContextProperty("translations", &translations);
        view.setSource(QUrl::fromLocalFile(pairingScreenSource()));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.show(); QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *qr = view.rootObject()->findChild<QQuickItem *>("pairingQr");
        auto *status = view.rootObject()->findChild<QQuickItem *>("pairingStatus");
        auto *spinner = view.rootObject()->findChild<QQuickItem *>("pairingActivity");
        QVERIFY(qr && status && spinner);
        const QString dir = qEnvironmentVariable("CONNECT_CAPTURE_DIR");
        auto capture = [&](const char *name) { if (!dir.isEmpty()) { QTest::qWait(80); QVERIFY(view.grabWindow().save(dir + "/service-" + QString::fromLatin1(QTest::currentDataTag()) + "-" + name + ".png")); } };
        service.setVisible(true);
        QVERIFY(service.checkingStatus());
        QVERIFY(!qr->isVisible()); capture("checking");
        QTRY_COMPARE(service.state(), QStringLiteral("waiting_for_web_setup"));
        QVERIFY(!service.running()); QVERIFY(service.canStart());
        QCOMPARE(status->property("text").toString(), translations.pairingReady());
        QVERIFY(!qr->isVisible()); capture("initial");
        repo.delayStatus = 0;
        emit input.rightTap();
        QCOMPARE(service.state(), QStringLiteral("starting"));
        QVERIFY(service.busy()); QVERIFY(!qr->isVisible()); QVERIFY(spinner->isVisible()); capture("start");
        QTRY_VERIFY(!service.busy());
        repo.snapshot = {{"state", "waiting_for_web_setup"}, {"running", true}};
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QStringLiteral("waiting_for_web_setup"), 3500);
        QCOMPARE(status->property("text").toString(), translations.pairingPreparingTitle());
        QVERIFY(!qr->isVisible()); capture("waiting");
        if (language == "en" && dark) {
            QTRY_VERIFY_WITH_TIMEOUT(view.rootObject()->property("slow").toBool(), 12000);
            QCOMPARE(view.rootObject()->property("stateBody").toString(), translations.pairingSlow());
            capture("slow");
        }
        repo.snapshot["code"] = "01234-ABCDE";
        repo.snapshot["expires_at"] = QDateTime::currentSecsSinceEpoch() + 900;
        QTRY_VERIFY_WITH_TIMEOUT(qr->isVisible(), 3500);
        QTRY_VERIFY(qAbs(view.rootObject()->property("now").toDouble() - QDateTime::currentMSecsSinceEpoch() / 1000.0) < 2);
        capture("qr");
        service.setInternetAvailable(false);
        QVERIFY(!qr->isVisible()); capture("offline");
        service.setInternetAvailable(true);
        QTRY_VERIFY_WITH_TIMEOUT(qr->isVisible(), 3500);
        emit input.leftHold();
        QVERIFY(nav.closed); QTRY_VERIFY(nav.resumed);
        QVERIFY(service.code().isEmpty());
        service.setVisible(true);
        QTRY_VERIFY_WITH_TIMEOUT(qr->isVisible(), 3500);
        repo.snapshot["expires_at"] = QDateTime::currentSecsSinceEpoch() - 1;
        QTRY_VERIFY_WITH_TIMEOUT(service.expired(), 3500);
        QVERIFY(!qr->isVisible()); capture("expired");
        repo.snapshot["expires_at"] = QDateTime::currentSecsSinceEpoch() + 900;
        QTRY_VERIFY_WITH_TIMEOUT(qr->isVisible(), 3500);
        repo.snapshot = {{"state", "stopping"}, {"running", true}};
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QStringLiteral("stopping"), 3500);
        QVERIFY(spinner->isVisible()); capture("stopping");
        repo.snapshot = {{"state", "applying"}, {"running", true}};
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QStringLiteral("applying"), 3500);
        QVERIFY(!qr->isVisible()); QVERIFY(spinner->isVisible()); capture("applying");
        repo.snapshot = {{"state", "connected"}, {"running", false}};
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QStringLiteral("connected"), 3500); capture("connected");
        repo.snapshot = {{"state", "invalid"}, {"running", false}};
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QStringLiteral("unavailable"), 3500); capture("error");
        auto *controls = view.rootObject()->findChild<QQuickItem *>("pairingControls"); QVERIFY(controls);
        QCOMPARE(controls->property("rightTap").toString(), translations.pairingRetry());
        repo.delayStatus = 1500;
        const int previousRequests = repo.statusRequests;
        QTRY_VERIFY_WITH_TIMEOUT(repo.statusRequests > previousRequests, 3500);
        QVERIFY(service.loadingStatus());
        QVERIFY(!service.checkingStatus());
        QCOMPARE(controls->property("rightTap").toString(), translations.pairingRetry());
        repo.snapshot = {{"state", "registered"}, {"running", false}};
        emit input.rightTap();
        QTRY_VERIFY(service.checkingStatus());
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QStringLiteral("registered"), 3500);
    }
    void backgroundFailureKeepsRetryVisible() {
        PairingTheme theme; PairingInput input; PairingScreenNavigation nav; Translations translations;
        ScreenPairingRepository repo;
        repo.snapshot = {{"state", "invalid"}, {"running", false}};
        CloudPairingService service(&repo);
        service.setInternetAvailable(true);
        QQuickView view;
        view.rootContext()->setContextProperty("themeStore", &theme);
        view.rootContext()->setContextProperty("inputHandler", &input);
        view.rootContext()->setContextProperty("cloudPairingService", &service);
        view.rootContext()->setContextProperty("screenStore", &nav);
        view.rootContext()->setContextProperty("menuStore", &nav);
        view.rootContext()->setContextProperty("translations", &translations);
        view.setSource(QUrl::fromLocalFile(pairingScreenSource()));
        QCOMPARE(view.status(), QQuickView::Ready);
        auto *controls = view.rootObject()->findChild<QQuickItem *>("pairingControls"); QVERIFY(controls);
        service.setVisible(true);
        QTRY_VERIFY(!service.checkingStatus());
        QCOMPARE(service.state(), QStringLiteral("unavailable"));
        QCOMPARE(controls->property("rightTap").toString(), translations.pairingRetry());
        repo.delayStatus = 1000;
        service.refresh();
        QVERIFY(service.loadingStatus());
        QVERIFY(!service.checkingStatus());
        QCOMPARE(controls->property("rightTap").toString(), translations.pairingRetry());
        repo.snapshot = {{"state", "registered"}, {"running", false}};
        emit input.rightTap();
        QVERIFY(service.checkingStatus());
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QStringLiteral("registered"), 2500);
    }
    void timedSequence() {
        PairingTheme theme; PairingInput input; PairingScreenModel model; PairingScreenNavigation nav; Translations translations;
        model.simulate = true; model.state = "registered"; model.running = false; model.canStart = true; model.canStop = false; model.code.clear();
        QQuickView view;
        view.resize(480, 480);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.rootContext()->setContextProperty("themeStore", &theme);
        view.rootContext()->setContextProperty("inputHandler", &input);
        view.rootContext()->setContextProperty("cloudPairingService", &model);
        view.rootContext()->setContextProperty("screenStore", &nav);
        view.rootContext()->setContextProperty("menuStore", &nav);
        view.rootContext()->setContextProperty("translations", &translations);
        view.setSource(QUrl::fromLocalFile(pairingScreenSource()));
        QCOMPARE(view.status(), QQuickView::Ready); view.show(); QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *qr = view.rootObject()->findChild<QQuickItem *>("pairingQr");
        auto *status = view.rootObject()->findChild<QQuickItem *>("pairingStatus");
        const QString dir = qEnvironmentVariable("CONNECT_CAPTURE_DIR");
        auto capture = [&](const char *name) { if (!dir.isEmpty()) { QTest::qWait(80); QVERIFY(view.grabWindow().save(dir + "/sequence-" + name + ".png")); } };
        QVERIFY(qr && status); QVERIFY(!qr->isVisible()); capture("initial");
        emit input.rightTap(); QCOMPARE(model.starts, 1); QCOMPARE(status->property("text").toString(), translations.pairingPreparingTitle()); capture("start");
        QTRY_COMPARE(model.state, QStringLiteral("waiting_for_web_setup")); QVERIFY(!qr->isVisible()); capture("waiting");
        QTRY_VERIFY(qr->isVisible()); capture("qr");
        model.state = "applying"; model.code.clear(); emit model.changed(); QVERIFY(!qr->isVisible()); capture("applying");
        model.state = "connected"; model.running = false; model.canStop = false; emit model.changed(); capture("connected");
        model.state = "error"; emit model.changed(); capture("error");
    }
    void staleCodeAndOfflineAreHidden() {
        PairingTheme theme; PairingInput input; PairingScreenModel model; PairingScreenNavigation nav; Translations translations;
        QQuickView view;
        view.rootContext()->setContextProperty("themeStore", &theme);
        view.rootContext()->setContextProperty("inputHandler", &input);
        view.rootContext()->setContextProperty("cloudPairingService", &model);
        view.rootContext()->setContextProperty("screenStore", &nav);
        view.rootContext()->setContextProperty("menuStore", &nav);
        view.rootContext()->setContextProperty("translations", &translations);
        view.setSource(QUrl::fromLocalFile(pairingScreenSource()));
        QCOMPARE(view.status(), QQuickView::Ready);
        auto *qr = view.rootObject()->findChild<QQuickItem *>("pairingQr");
        auto *status = view.rootObject()->findChild<QQuickItem *>("pairingStatus");
        QVERIFY(qr && status);
        model.running = false; emit model.changed();
        QVERIFY(!qr->isVisible());
        QCOMPARE(status->property("text").toString(), translations.pairingReady());
        model.running = true; model.code.clear(); emit model.changed();
        QVERIFY(!qr->isVisible());
        QCOMPARE(status->property("text").toString(), translations.pairingPreparingTitle());
        model.code = "01234-ABCDE"; model.online = false; emit model.changed();
        QVERIFY(!qr->isVisible());
        QCOMPARE(status->property("text").toString(), translations.pairingOfflineTitle());
    }
    void render_data() {
        QTest::addColumn<QString>("language"); QTest::addColumn<bool>("dark"); QTest::addColumn<QString>("state"); QTest::addColumn<QString>("code");
        for (const auto &language : {QString("en"), QString("de")})
            for (bool dark : {true, false})
                for (const auto &state : {QString("registered"), QString("waiting_for_web_setup"), QString("starting"), QString("applying"), QString("stopping"), QString("connected"), QString("error"), QString("paused"), QString("offline"), QString("expired")})
                    QTest::newRow(qPrintable(language + (dark ? "-dark-" : "-light-") + state)) << language << dark << state << QString("WWWWW-WWWWW");
        QTest::newRow("en-dark-code2") << QString("en") << true << QString("waiting_for_web_setup") << QString("01234-ABCDE");
        QTest::newRow("de-light-code3") << QString("de") << false << QString("waiting_for_web_setup") << QString("6789A-BCDEF");
    }
    void render() {
        QFETCH(QString, language); QFETCH(bool, dark); QFETCH(QString, state); QFETCH(QString, code);
        PairingTheme theme; theme.dark = dark;
        PairingInput input; PairingScreenModel model; PairingScreenNavigation nav; Translations translations;
        translations.setLanguage(language); model.state = state; model.code = code;
        if (state != "waiting_for_web_setup") model.code.clear();
        if (state == "registered") { model.running = false; model.canStop = false; model.canStart = true; }
        if (state == "applying") model.canStop = false;
        if (state == "offline") { model.online = false; model.canStop = false; }
        if (state == "expired") { model.state = "waiting_for_web_setup"; model.expired = true; }
        QFontDatabase::addApplicationFont(QFINDTESTDATA("../assets/fonts/subset/RobotoCondensed-Regular.ttf"));
        QFontDatabase::addApplicationFont(QFINDTESTDATA("../assets/fonts/subset/Roboto-Regular.ttf"));
        QQuickView view;
        view.setResizeMode(QQuickView::SizeRootObjectToView); view.resize(480,480);
        view.rootContext()->setContextProperty("themeStore", &theme);
        view.rootContext()->setContextProperty("inputHandler", &input);
        view.rootContext()->setContextProperty("cloudPairingService", &model);
        view.rootContext()->setContextProperty("screenStore", &nav);
        view.rootContext()->setContextProperty("menuStore", &nav);
        view.rootContext()->setContextProperty("translations", &translations);
        view.setSource(QUrl::fromLocalFile(pairingScreenSource()));
        QCOMPARE(view.status(), QQuickView::Ready); view.show();
        auto *qr = view.rootObject()->findChild<QQuickItem*>("pairingQr"); QVERIFY(qr);
        QCOMPARE(qr->isVisible(), !model.code.isEmpty());
        auto *safety = view.rootObject()->findChild<QQuickItem*>("pairingSafety");
        auto *controls = view.rootObject()->findChild<QQuickItem*>("pairingControls"); QVERIFY(safety); QVERIFY(controls);
        QTRY_VERIFY(safety->mapRectToScene(safety->boundingRect()).bottom() <= controls->mapRectToScene(controls->boundingRect()).top() - 2);
        for (const auto &name : {"pairingTitle", "pairingInstructions", "pairingCode", "pairingExpiry", "pairingStatus", "pairingSafety"}) {
            auto *text = view.rootObject()->findChild<QQuickItem*>(name); QVERIFY(text);
            const QRectF bounds = text->mapRectToScene(text->boundingRect());
            if (!text->property("text").toString().isEmpty())
                QVERIFY2(QRectF(0,0,480,480).contains(bounds), name);
            QVERIFY2(!text->property("truncated").toBool(), name);
        }
        auto *codeItem = view.rootObject()->findChild<QQuickItem*>("pairingCode");
        QCOMPARE(codeItem->property("text").toString(), model.code);
        const auto directory = qEnvironmentVariable("CONNECT_CAPTURE_DIR");
        if (!directory.isEmpty()) {
            QDir().mkpath(directory);
            QTest::qWait(80);
            QTRY_VERIFY(!view.grabWindow().isNull());
            QVERIFY(view.grabWindow().save(directory + "/" + QString::fromLatin1(QTest::currentDataTag()) + ".png"));
        }
        QCOMPARE(model.starts, 0);
        emit input.rightTap();
        QCOMPARE(model.stops, model.canStop && state != "applying" && state != "connected" ? 1 : 0);
        const int previousStarts = model.starts;
        model.canStop = false; model.canStart = true; model.online = true; emit model.changed();
        emit input.rightTap(); QCOMPARE(model.starts, previousStarts + 1);
        emit input.leftHold(); QVERIFY(nav.closed); QTRY_VERIFY(nav.resumed);
    }
};
QTEST_MAIN(CloudPairingScreenTest)
#include "CloudPairingScreenTest.moc"

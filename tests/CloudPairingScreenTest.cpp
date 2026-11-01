#include <QtTest>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickItem>
#include <QFontDatabase>
#include <QDir>
#include "l10n/Translations.h"
#include "services/PairingQrItem.h"

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
public:
    QString state = "waiting_for_web_setup", code = "WWWWW-WWWWW";
    qint64 expiresAt = QDateTime::currentSecsSinceEpoch() + 900;
    bool canStart = false, canStop = true;
    int starts = 0, stops = 0;
    Q_INVOKABLE void start() { ++starts; }
    Q_INVOKABLE void stop() { ++stops; }
signals:
    void changed();
};
class PairingScreenNavigation : public QObject {
    Q_OBJECT
public:
    bool closed = false, resumed = false;
    Q_INVOKABLE void closeCloudPairing() { closed = true; }
    Q_INVOKABLE void resume() { resumed = true; }
};
class CloudPairingScreenTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qmlRegisterType<PairingQrItem>("ScootUI.Pairing", 1, 0, "PairingQr"); }
    void render_data() {
        QTest::addColumn<QString>("language"); QTest::addColumn<bool>("dark"); QTest::addColumn<QString>("state"); QTest::addColumn<QString>("code");
        for (const auto &language : {QString("en"), QString("de")})
            for (bool dark : {true, false})
                for (const auto &state : {QString("waiting_for_web_setup"), QString("starting"), QString("stopping"), QString("connected"), QString("error"), QString("paused")})
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
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../qml/screens/CloudPairingScreen.qml")));
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
            QTRY_VERIFY(!view.grabWindow().isNull());
            QVERIFY(view.grabWindow().save(directory + "/" + QString::fromLatin1(QTest::currentDataTag()) + ".png"));
        }
        QCOMPARE(model.starts, 0);
        emit input.rightTap(); QCOMPARE(model.stops, 1);
        model.canStop = false; model.canStart = true; emit model.changed();
        emit input.rightTap(); QCOMPARE(model.starts, 1);
        emit input.leftHold(); QVERIFY(nav.closed); QVERIFY(nav.resumed);
    }
};
QTEST_MAIN(CloudPairingScreenTest)
#include "CloudPairingScreenTest.moc"

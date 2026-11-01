#include <QtTest>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QQuickItem>
#include "l10n/Translations.h"

class SmsFakeTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDark MEMBER dark CONSTANT)
    Q_PROPERTY(int fontHeading MEMBER heading CONSTANT)
    Q_PROPERTY(int fontTitle MEMBER title CONSTANT)
    Q_PROPERTY(int fontBody MEMBER body CONSTANT)
    Q_PROPERTY(int fontCaption MEMBER caption CONSTANT)
    Q_PROPERTY(int fontMicro MEMBER micro CONSTANT)
public:
    bool dark = true;
    int heading = 28, title = 24, body = 20, caption = 16, micro = 10;
};
class SmsFakeInput : public QObject {
    Q_OBJECT
signals:
    void leftTap();
    void rightTap();
    void leftHold();
    void rightHold();
};
class SmsFakeStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap openedMessage MEMBER message CONSTANT)
    Q_PROPERTY(bool connected MEMBER connected CONSTANT)
    Q_PROPERTY(bool openedAvailable MEMBER available CONSTANT)
public:
    QVariantMap message;
    bool connected = true, available = true;
    int deletes = 0;
    Q_INVOKABLE bool deleteOpenedMessage() { ++deletes; return true; }
};
class SmsFakeNavigation : public QObject {
    Q_OBJECT
public:
    bool closed = false, resumed = false;
    Q_INVOKABLE void closeSmsInbox() { closed = true; }
    Q_INVOKABLE void resume() { resumed = true; }
};
class SmsInboxScreenTest : public QObject {
    Q_OBJECT
private slots:
    void reader_data();
    void reader();
};
void SmsInboxScreenTest::reader_data() {
    QTest::addColumn<QString>("language");
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("longText");
    for (const QString &lang : {QStringLiteral("de"), QStringLiteral("en")})
        for (bool dark : {false, true})
            for (bool longText : {false, true})
                QTest::newRow(qPrintable(QStringLiteral("%1-%2-%3").arg(lang, dark ? "dark" : "light", longText ? "long" : "short")))
                    << lang << dark << longText;
}
void SmsInboxScreenTest::reader() {
    QFETCH(QString, language);
    QFETCH(bool, dark);
    QFETCH(bool, longText);
    SmsFakeTheme theme;
    theme.dark = dark;
    SmsFakeInput input;
    SmsFakeStore store;
    store.message = {{"from", ""}, {"timestamp", "2026-11-01T09:15:00Z"},
        {"text", longText ? QStringLiteral("<b>Ihr Datenvolumen ist fast aufgebraucht.</b>\n").repeated(30)
                          : QStringLiteral("<b>Ihr Datenvolumen ist fast aufgebraucht.</b>")}};
    SmsFakeNavigation nav;
    Translations translations;
    translations.setLanguage(language);
    QQuickView view;
    QSignalSpy warnings(view.engine(), &QQmlEngine::warnings);
    view.rootContext()->setContextProperty("themeStore", &theme);
    view.rootContext()->setContextProperty("translations", &translations);
    view.rootContext()->setContextProperty("inputHandler", &input);
    view.rootContext()->setContextProperty("smsStore", &store);
    view.rootContext()->setContextProperty("screenStore", &nav);
    view.rootContext()->setContextProperty("menuStore", &nav);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(480, 480);
    QString source = qEnvironmentVariable("SCOOTUI_SMS_SCREEN_SOURCE");
    if (source.isEmpty()) source = QFINDTESTDATA("../qml/screens/SmsInboxScreen.qml");
    QVERIFY(!source.isEmpty());
    view.setSource(QUrl::fromLocalFile(source));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *root = view.rootObject();
    auto *title = root->findChild<QQuickItem *>("smsReaderTitle");
    auto *sender = root->findChild<QQuickItem *>("smsSender");
    auto *body = root->findChild<QQuickItem *>("smsBody");
    auto *scroll = root->findChild<QQuickItem *>("smsBodyScroll");
    QVERIFY(title && sender && body && scroll);
    QCOMPARE(title->property("text").toString(), language == "de" ? QStringLiteral("Nachricht") : QStringLiteral("Message"));
    QCOMPARE(sender->property("text").toString(), translations.smsUnknownSender());
    QCOMPARE(body->property("text").toString(), store.message.value("text").toString());
    QCOMPARE(body->property("textFormat").toInt(), 0); // PlainText, including markup in received SMS.
    QVERIFY(scroll->height() > 250);
    QVERIFY(scroll->mapToScene(QPointF(0, scroll->height())).y() <= 427);
    QTRY_COMPARE(root->property("canScrollDown").toBool(), longText);
    if (longText) {
        emit input.leftTap();
        QTRY_VERIFY(scroll->property("contentY").toDouble() > 100);
        QVERIFY(root->property("canScrollUp").toBool());
        emit input.rightHold();
        QTRY_COMPARE(scroll->property("contentY").toDouble(), 0.0);
    }
    const QString captureDir = qEnvironmentVariable("SCOOTUI_TEST_CAPTURE_DIR");
    if (!captureDir.isEmpty()) {
        QDir().mkpath(captureDir);
        const auto image = view.grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY(image.save(captureDir + "/sms-reader-" + QLatin1String(QTest::currentDataTag()) + ".png"));
    }
    translations.setLanguage(language == "de" ? "en" : "de");
    QTRY_COMPARE(title->property("text").toString(), translations.smsMessage());
    QTRY_COMPARE(sender->property("text").toString(), translations.smsUnknownSender());
    translations.setLanguage(language);
    if (longText) { emit input.leftTap(); QTRY_VERIFY(scroll->property("contentY").toDouble() > 100); }
    const double savedPosition = scroll->property("contentY").toDouble();
    emit input.rightTap();
    QVERIFY(root->property("confirming").toBool());
    QVERIFY(!root->property("confirmDelete").toBool());
    if (!captureDir.isEmpty()) QVERIFY(view.grabWindow().save(captureDir + "/sms-confirm-" + QLatin1String(QTest::currentDataTag()) + ".png"));
    emit input.rightTap();
    QVERIFY(!root->property("confirming").toBool());
    QCOMPARE(scroll->property("contentY").toDouble(), savedPosition);
    QCOMPARE(store.deletes, 0);
    emit input.rightTap();
    emit input.leftTap();
    QVERIFY(root->property("confirmDelete").toBool());
    emit input.rightTap();
    QCOMPARE(store.deletes, 1);
    QTRY_VERIFY(nav.closed && nav.resumed);
    QVERIFY(warnings.isEmpty());
}
QTEST_MAIN(SmsInboxScreenTest)
#include "SmsInboxScreenTest.moc"

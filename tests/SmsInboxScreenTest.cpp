#include <QtTest>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickItem>

class SmsFakeTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDark READ isDark CONSTANT)
    Q_PROPERTY(int fontBody READ fontBody CONSTANT)
    Q_PROPERTY(int fontCaption READ fontCaption CONSTANT)
public:
    bool isDark() const { return true; }
    int fontBody() const { return 20; }
    int fontCaption() const { return 14; }
};
class SmsFakeInput : public QObject {
    Q_OBJECT
signals:
    void leftTap();
    void rightTap();
    void leftHold();
    void rightHold();
    void leftDoubleTap();
};
class SmsFakeStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList messages READ messages CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool connected READ connected CONSTANT)
    Q_PROPERTY(QString forwardTo READ forwardTo CONSTANT)
public:
    QString marked, deleted;
    bool disabled = false;
    QVariantList messages() const { return {QVariantMap{{"id", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}, {"from", "+49123"}, {"text", "hello"}, {"timestamp", "2026-01-01"}, {"read", false}}}; }
    bool available() const { return true; }
    bool connected() const { return true; }
    QString forwardTo() const { return "+4915112345678"; }
    Q_INVOKABLE void markRead(const QString &id) { marked = id; }
    Q_INVOKABLE void deleteMessage(const QString &id) { deleted = id; }
    Q_INVOKABLE void disableForwarding() { disabled = true; }
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
    void gestures();
};
void SmsInboxScreenTest::gestures() {
    SmsFakeTheme theme;
    SmsFakeInput input;
    SmsFakeStore store;
    SmsFakeNavigation nav;
    QQuickView view;
    view.rootContext()->setContextProperty("themeStore", &theme);
    view.rootContext()->setContextProperty("inputHandler", &input);
    view.rootContext()->setContextProperty("smsStore", &store);
    view.rootContext()->setContextProperty("screenStore", &nav);
    view.rootContext()->setContextProperty("menuStore", &nav);
    view.resize(480, 480);
    const auto source = QFINDTESTDATA("../qml/screens/SmsInboxScreen.qml");
    QVERIFY(!source.isEmpty());
    view.setSource(QUrl::fromLocalFile(source));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QCoreApplication::processEvents();
    emit input.rightTap();
    QCOMPARE(store.marked, QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    emit input.rightTap();
    QVERIFY(view.rootObject()->property("confirmingDelete").toBool());
    emit input.leftHold();
    QVERIFY(!view.rootObject()->property("confirmingDelete").toBool());
    emit input.rightTap();
    emit input.rightTap();
    QCOMPARE(store.deleted, store.marked);
    emit input.leftDoubleTap();
    QVERIFY(store.disabled);
    emit input.leftHold();
    QVERIFY(nav.closed);
    QVERIFY(nav.resumed);
}
QTEST_MAIN(SmsInboxScreenTest)
#include "SmsInboxScreenTest.moc"

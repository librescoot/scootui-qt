#include <QtTest>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickItem>
#include <QFontDatabase>
#include <QRawFont>
#include <QDir>
#include <QJSValue>
#include "l10n/Translations.h"

class MapTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDark MEMBER dark CONSTANT)
    Q_PROPERTY(int fontBody READ fontBody CONSTANT)
    Q_PROPERTY(int fontCaption READ fontCaption CONSTANT)
    Q_PROPERTY(int fontMicro READ fontMicro CONSTANT)
public:
    bool dark = false;
    int fontBody() const { return 18; }
    int fontCaption() const { return 13; }
    int fontMicro() const { return 10; }
};
class MapScreen : public QObject {
    Q_OBJECT
    Q_PROPERTY(int systemInfoPage READ page CONSTANT)
public:
    int page() const { return 3; }
};
class MapInput : public QObject {
    Q_OBJECT
signals:
    void leftTap();
    void leftHold();
    void rightHold();
};
class MapModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap mapInfo MEMBER info NOTIFY changed)
public:
    QVariantMap info;
signals:
    void changed();
};

class MapInfoScreenTest : public QObject {
    Q_OBJECT
    static QVariantList rows(QQuickItem *root, const char *name) {
        return root->property(name).value<QJSValue>().toVariant().toList();
    }
    static QString field(const QVariantList &list, int row, const char *name) {
        return list.at(row).toMap().value(name).toString();
    }
    static QList<QQuickItem *> items(QQuickItem *root, const QString &name) {
        QList<QQuickItem *> result;
        if (root->objectName() == name) result.append(root);
        for (auto *child : root->childItems()) result.append(items(child, name));
        return result;
    }
private slots:
    void render_data() {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }
    void render() {
        QFETCH(bool, dark);
        MapTheme theme; theme.dark = dark;
        MapScreen screen; MapInput input; MapModel model; Translations translations;
        model.info = {{"region-name", "Berlin & Brandenburg"}, {"last-update-check", "2026-10-25T12:00:00Z"},
                      {"map:size", "210000000"}, {"map:published-at", "2026-10-05T12:00:00Z"},
                      {"map:mtime", "2026-10-05"}, {"map:sha256", "da197cda57ee123456"},
                      {"routing:size", "200000000"}, {"routing:published-at", "2026-10-05"},
                      {"routing:mtime", "2026-10-05"}, {"routing:sha256", "0123456789abcdef"}};
        const auto regular = QFINDTESTDATA("../assets/fonts/subset/Roboto-Regular.ttf");
        const auto bold = QFINDTESTDATA("../assets/fonts/subset/Roboto-Bold.ttf");
        QVERIFY(QFontDatabase::addApplicationFont(regular) >= 0);
        QVERIFY(QFontDatabase::addApplicationFont(bold) >= 0);
        for (const auto &path : {regular, bold}) {
            QRawFont font(path, 18);
            QVERIFY(font.isValid());
            for (const auto ch : QString::fromUtf8("üößÜä")) QVERIFY(font.supportsCharacter(ch));
        }
        QQuickView view;
        view.setResizeMode(QQuickView::SizeRootObjectToView); view.resize(480, 480);
        view.rootContext()->setContextProperty("themeStore", &theme);
        view.rootContext()->setContextProperty("screenStore", &screen);
        view.rootContext()->setContextProperty("inputHandler", &input);
        view.rootContext()->setContextProperty("mapDownloadService", &model);
        view.rootContext()->setContextProperty("translations", &translations);
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../qml/screens/SystemInfoScreen.qml")));
        QCOMPARE(view.status(), QQuickView::Ready); view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *root = view.rootObject();
        auto *flick = root->findChild<QQuickItem *>("infoFlickable");
        auto *footer = root->findChild<QQuickItem *>("infoControlBar");
        QVERIFY(flick); QVERIFY(footer);
        QCOMPARE(root->width(), 480); QCOMPARE(root->height(), 480);
        QCOMPARE(flick->y() + flick->height(), footer->y());
        QVERIFY(root->property("canScrollDown").toBool());
        const auto captureDir = qEnvironmentVariable("MAP_INFO_CAPTURE_DIR");
        if (!captureDir.isEmpty()) QVERIFY(QDir().mkpath(captureDir));
        for (const auto &language : {QString("en"), QString("de"), QString("en")}) {
            translations.setLanguage(language);
            const bool de = language == "de";
            QTRY_COMPARE(field(rows(root, "mapRegionRows"), 1, "label"), de ? QString::fromUtf8("Zuletzt geprüft") : QString("Last checked"));
            auto region = rows(root, "mapRegionRows");
            QCOMPARE(field(region, 0, "label"), QString("Region"));
            QCOMPARE(field(region, 0, "value"), QString("Berlin & Brandenburg"));
            QCOMPARE(field(region, 1, "value"), QString("2026-10-25"));
            for (const auto *name : {"mapDisplayRows", "mapRoutingRows"}) {
                auto tile = rows(root, name); QCOMPARE(tile.size(), 4);
                QCOMPARE(field(tile, 0, "label"), de ? QString::fromUtf8("Veröffentlicht") : QString("Published"));
                QCOMPARE(field(tile, 1, "label"), de ? QString("Installiert") : QString("Installed"));
                QCOMPARE(field(tile, 2, "label"), de ? QString::fromUtf8("Größe") : QString("Size"));
                QCOMPARE(field(tile, 3, "label"), de ? QString::fromUtf8("Prüfsumme") : QString("Checksum"));
                QCOMPARE(field(tile, 0, "value"), QString("2026-10-05"));
                QCOMPARE(field(tile, 1, "value"), QString("2026-10-05"));
            }
            QCOMPARE(field(rows(root, "mapDisplayRows"), 2, "value"), QString("210 MB"));
            QCOMPARE(field(rows(root, "mapDisplayRows"), 3, "value"), QString("da197cda57ee"));
            flick->setProperty("contentY", 0); QTest::qWait(100);
            const auto labels = items(root, "infoRowLabel");
            QVERIFY(!labels.isEmpty());
            for (auto *label : labels) {
                if (!label->isVisible()) continue;
                QVERIFY(label->property("implicitWidth").toReal() <= label->width());
                const auto values = items(label->parentItem(), "infoRowValue");
                QCOMPARE(values.size(), 1);
                auto *value = values.first();
                QVERIFY2(!value->property("truncated").toBool(), qPrintable(value->property("text").toString()));
            }
            if (!captureDir.isEmpty())
                QVERIFY(view.grabWindow().save(captureDir + "/" + language + (dark ? "-dark" : "-light") + "-top.png"));
            emit input.leftTap(); QTest::qWait(300);
            QVERIFY(root->property("canScrollUp").toBool());
            QCOMPARE(flick->y() + flick->height(), footer->y());
            if (!captureDir.isEmpty())
                QVERIFY(view.grabWindow().save(captureDir + "/" + language + (dark ? "-dark" : "-light") + "-scrolled.png"));
            emit input.rightHold(); QTest::qWait(300);
            QVERIFY(!root->property("canScrollUp").toBool());
        }
        model.info.insert("update-available", "true");
        model.info.insert("map:update-available", "true");
        model.info.remove("routing:size"); emit model.changed();
        for (const auto &language : {QString("de"), QString("en"), QString("de")}) {
            translations.setLanguage(language);
            const bool de = language == "de";
            const auto suffix = de ? QString::fromUtf8("Update verfügbar") : QString("Update available");
            QTRY_VERIFY(field(rows(root, "mapRegionRows"), 1, "value").endsWith(suffix));
            QVERIFY(field(rows(root, "mapDisplayRows"), 0, "value").endsWith(suffix));
            auto routing = rows(root, "mapRoutingRows"); QCOMPARE(routing.size(), 1);
            QCOMPARE(field(routing, 0, "label"), QString("Status"));
            QCOMPARE(field(routing, 0, "value"), de ? QString("Nicht installiert") : QString("Not installed"));
            QTRY_COMPARE(items(root, "wrappedInfoValue").size(), 2);
            const auto wrapped = items(root, "wrappedInfoValue");
            for (auto *value : wrapped) {
                QVERIFY(!value->property("truncated").toBool());
                QVERIFY(value->height() <= value->parentItem()->height());
            }
            if (!captureDir.isEmpty()) {
                flick->setProperty("contentY", 0); QTest::qWait(100);
                QVERIFY(view.grabWindow().save(captureDir + "/" + language + (dark ? "-dark" : "-light") + "-update.png"));
            }
        }
    }
};
QTEST_MAIN(MapInfoScreenTest)
#include "MapInfoScreenTest.moc"

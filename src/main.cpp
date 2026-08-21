#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>
#include <exception>

#include "core/EnvConfig.h"
#include "core/Application.h"
#include "routing/RouteModels.h"

// Global boot timer — logged via BOOT_MARK() from main() and createStores()
// to pinpoint where startup time goes. Grep journal for '\[boot \+'.
QElapsedTimer g_bootTimer;
#define BOOT_MARK(what) \
    qDebug().nospace().noquote() << QStringLiteral("[boot +%1ms] %2").arg(g_bootTimer.elapsed(), 5).arg(QStringLiteral(what))

int main(int argc, char *argv[])
{
    g_bootTimer.start();
    BOOT_MARK("main() entered");

    qRegisterMetaType<Route>("Route");

    QGuiApplication app(argc, argv);
    BOOT_MARK("QGuiApplication ready");

    app.setApplicationName(QStringLiteral("ScootUI"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    // Load bundled Roboto fonts (matching Flutter's default Material font)
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/Roboto-Regular.ttf"));
    BOOT_MARK("font: Roboto-Regular");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/Roboto-Bold.ttf"));
    BOOT_MARK("font: Roboto-Bold");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/Roboto-Medium.ttf"));
    BOOT_MARK("font: Roboto-Medium");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/Roboto-Light.ttf"));
    BOOT_MARK("font: Roboto-Light");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/RobotoCondensed-Bold.ttf"));
    BOOT_MARK("font: RobotoCondensed-Bold");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/RobotoCondensed-Regular.ttf"));
    BOOT_MARK("font: RobotoCondensed-Regular");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/MaterialIcons-Regular.otf"));
    BOOT_MARK("font: MaterialIcons");

    // Set Roboto as default application font
    QFont defaultFont(QStringLiteral("Roboto"));
    defaultFont.setPixelSize(16);
    app.setFont(defaultFont);

    EnvConfig::initialize();

    QQmlApplicationEngine engine;
    BOOT_MARK("QQmlApplicationEngine ready");

    // Ensure QMapLibre QML modules (MapLibre.Location) are found
    engine.addImportPath(QStringLiteral("/usr/local/qml"));
    engine.addImportPath(QStringLiteral("/usr/qml"));

    Application application;
    BOOT_MARK("Application::initialize starting");
    if (!application.initialize(engine)) {
        qCritical() << "Failed to initialize application";
        return 1;
    }
    BOOT_MARK("Application::initialize done");

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    // Hand the display over to us only once a frame is actually on screen.
    // objectCreated fires ~2s before the first swap on the DBC, and on kernels
    // without /sys/class/graphics/fb1/overlay_alpha the handoff is an immediate
    // stop of boot-animation rather than a 1s fade — so triggering it there
    // leaves a visible gap between the splash going away and the cluster
    // appearing.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &application, [&application](QObject *obj, const QUrl &) {
            BOOT_MARK("QML objectCreated");
            if (!obj)
                return;
            auto *window = qobject_cast<QQuickWindow*>(obj);
            if (!window) {
                application.uiPresented();
                return;
            }
            QObject::connect(window, &QQuickWindow::frameSwapped,
                &application, [&application]() {
                    BOOT_MARK("first frameSwapped");
                    application.uiPresented();
                },
                Qt::SingleShotConnection);
        },
        Qt::QueuedConnection);

    const QUrl url(QStringLiteral("qrc:/ScootUI/qml/Main.qml"));
    BOOT_MARK("engine.load() starting");
    if (qEnvironmentVariableIsSet("SCOOTUI_SPLIT_LOAD")) {
        // Same work engine.load() does, in two halves, to see which one costs.
        // Compiling resolves every type Main.qml names, including the screen
        // Components it never instantiates; creating walks the object graph.
        // Component.onCompleted cannot tell these apart because it fires in a
        // batch once the whole tree exists.
        QQmlComponent component(&engine);
        component.loadUrl(url);
        BOOT_MARK("  QML compiled + types resolved");
        if (component.isError()) {
            qCritical() << "QML errors:" << component.errors();
            return 1;
        }
        QObject *root = component.create(engine.rootContext());
        BOOT_MARK("  QML object graph created");
        if (root)
            root->setParent(&engine);
    } else {
        engine.load(url);
    }
    BOOT_MARK("engine.load() returned");

    // In simulator mode, also load the simulator control panel window
    if (application.isSimulatorMode()) {
        const QUrl simUrl(QStringLiteral("qrc:/ScootUI/qml/simulator/SimulatorWindow.qml"));
        engine.load(simUrl);
    }

    // TEMPORARY DIAGNOSTIC: dump every running QML animation with the file it
    // came from. Qt Quick keeps rendering at full frame rate while any
    // animation runs, even one whose target is invisible.
    if (qEnvironmentVariableIsSet("SCOOTUI_DUMP_ANIMATIONS")) {
        auto *dumpTimer = new QTimer(&app);
        QObject::connect(dumpTimer, &QTimer::timeout, [&engine]() {
            int n = 0;
            for (QObject *root : engine.rootObjects()) {
                for (QObject *o : root->findChildren<QObject *>()) {
                    const QByteArray cls = o->metaObject()->className();
                    if (!cls.contains("Animation") && !cls.contains("Animator"))
                        continue;
                    const QVariant r = o->property("running");
                    if (!r.isValid() || !r.toBool())
                        continue;
                    QString where;
                    if (QQmlContext *c = qmlContext(o))
                        where = c->baseUrl().toString();
                    qInfo().noquote() << "RUNNING-ANIM" << cls << where;
                    ++n;
                }
            }
            qInfo().noquote() << "RUNNING-ANIM-TOTAL" << n;
        });
        dumpTimer->start(3000);
    }

    // Safety net: catch exceptions from MapLibre's internal sqlite reader
    // (e.g. mapbox::sqlite::Exception on malformed mbtiles databases)
    try {
        return app.exec();
    } catch (const std::exception &e) {
        qCritical() << "Unhandled exception:" << e.what();
        return 1;
    }
}

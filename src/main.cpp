#include <QGuiApplication>
#include <QQmlApplicationEngine>
#if __has_include(<main.h>)
// qmltc emits one C++ class per QML type into .qmltc/<target>/. Including the
// root lets us instantiate the compiled Main instead of interpreting Main.qml.
#  include <main.h>
#  define SCOOTUI_HAVE_QMLTC 1
#endif
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>
#include <QElapsedTimer>
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
    // The ScootUI module's qmldir lives at :/ScootUI/qmldir (resource prefix "/",
    // since QTP0001 is OLD here). Without this the engine resolves the module
    // from the C++ registry only, and the composite QML types are invisible.
    engine.addImportPath(QStringLiteral("qrc:/"));
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
#ifdef SCOOTUI_HAVE_QMLTC
    // Enabling ENABLE_TYPE_COMPILER only *builds* the compiled classes. Nothing
    // uses them unless the root is constructed from C++ like this; engine.load()
    // goes on interpreting the QML, which is why turning the flag on alone
    // changed nothing on the DBC (844ms vs 842ms for engine.load).
    //
    // Opt-in: the compiled root segfaults while a Loader completes, inside
    // QQmlObjectCreator::create reached from Main::QML_completeComponent. It is
    // not the asynchronous flag (it happens with every Loader synchronous) and
    // not an outer-id reference from an inline Component (moving those bindings
    // out relocates the crash rather than clearing it). qmltc is a Tech Preview
    // that "might not compile an arbitrary QML program", and Main.qml drives
    // fifteen Loaders off inline Components, so this needs an upstream answer
    // or a restructure of the screen switcher, not a QML tweak.
    if (qEnvironmentVariableIsSet("SCOOTUI_QMLTC")) {
        // parent goes through the constructor: QQuickWindow::setParent takes a
        // QWindow*, so the QObject overload is not reachable here.
        auto *root = new ScootUI::Main(&engine, &engine);
        BOOT_MARK("qmltc root constructed");
        // objectCreated never fires for a type-compiled root, so the display
        // handoff is wired here instead.
        QObject::connect(root, &QQuickWindow::frameSwapped, &application,
            [&application]() {
                BOOT_MARK("first frameSwapped");
                application.uiPresented();
            },
            Qt::SingleShotConnection);
        root->setVisible(true);
    } else
#endif
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

    // Safety net: catch exceptions from MapLibre's internal sqlite reader
    // (e.g. mapbox::sqlite::Exception on malformed mbtiles databases)
    try {
        return app.exec();
    } catch (const std::exception &e) {
        qCritical() << "Unhandled exception:" << e.what();
        return 1;
    }
}

#include <QGuiApplication>
#include <QQmlApplicationEngine>
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
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/Roboto-Regular.ttf"));
    BOOT_MARK("font: Roboto-Regular");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/Roboto-Bold.ttf"));
    BOOT_MARK("font: Roboto-Bold");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/Roboto-Medium.ttf"));
    BOOT_MARK("font: Roboto-Medium");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/Roboto-Light.ttf"));
    BOOT_MARK("font: Roboto-Light");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/RobotoCondensed-Bold.ttf"));
    BOOT_MARK("font: RobotoCondensed-Bold");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/RobotoCondensed-Regular.ttf"));
    BOOT_MARK("font: RobotoCondensed-Regular");
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/MaterialIcons-Regular.otf"));
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

    // Boot animation: fade in overlay after QML loads
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &application, [&application](QObject *obj, const QUrl &) {
            BOOT_MARK("QML objectCreated");
            if (obj) {
                application.fadeInOverlay();
                if (auto *window = qobject_cast<QQuickWindow*>(obj)) {
                    QObject::connect(window, &QQuickWindow::frameSwapped,
                        &application, []() {
                            BOOT_MARK("first frameSwapped");
                        },
                        Qt::SingleShotConnection);
                }
            }
        },
        Qt::QueuedConnection);

    const QUrl url(QStringLiteral("qrc:/ScootUI/qml/Main.qml"));
    BOOT_MARK("engine.load() starting");
    engine.load(url);
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

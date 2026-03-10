#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QFontDatabase>
#include <QFont>

#include "core/EnvConfig.h"
#include "core/Application.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ScootUI"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    // Load bundled Roboto fonts (matching Flutter's default Material font)
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/Roboto-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/Roboto-Bold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/Roboto-Medium.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/Roboto-Light.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/RobotoCondensed-Bold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/RobotoCondensed-Regular.ttf"));

    // Set Roboto as default application font
    QFont defaultFont(QStringLiteral("Roboto"));
    defaultFont.setPixelSize(16);
    app.setFont(defaultFont);

    EnvConfig::initialize();

    QQmlApplicationEngine engine;

    Application application;
    if (!application.initialize(engine)) {
        qCritical() << "Failed to initialize application";
        return 1;
    }

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    // Boot animation: fade in overlay after QML loads
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &application, [&application](QObject *obj, const QUrl &) {
            if (obj) {
                application.fadeInOverlay();
            }
        },
        Qt::QueuedConnection);

    const QUrl url(QStringLiteral("qrc:/ScootUI/qml/Main.qml"));
    engine.load(url);

    // In simulator mode, also load the simulator control panel window
    if (application.isSimulatorMode()) {
        const QUrl simUrl(QStringLiteral("qrc:/ScootUI/qml/simulator/SimulatorWindow.qml"));
        engine.load(simUrl);
    }

    return app.exec();
}

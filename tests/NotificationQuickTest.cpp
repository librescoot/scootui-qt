#include <QtQuickTest/quicktest.h>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>

class NotificationQuickTestSetup : public QObject
{
    Q_OBJECT
public slots:
    void applicationAvailable()
    {
        for (const auto *font : {"Roboto-Regular.ttf", "Roboto-Bold.ttf", "Roboto-Medium.ttf",
                                 "MaterialIcons-Regular.otf"})
            QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/") + font);
        QGuiApplication::setFont(QFont(QStringLiteral("Roboto")));
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        const QString directory = qEnvironmentVariable("SCOOTUI_TEST_CAPTURE_DIR");
        if (!directory.isEmpty())
            QDir().mkpath(directory);
        engine->rootContext()->setContextProperty(QStringLiteral("captureDirectory"), directory);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(notification, NotificationQuickTestSetup)
#include "NotificationQuickTest.moc"

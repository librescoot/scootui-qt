#include <QtQml/QQmlContext>
#include <QtQml/qqml.h>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include "models/Enums.h"

class ThemeMock : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int fontBody READ fontBody CONSTANT)
    Q_PROPERTY(int fontTitle READ fontTitle CONSTANT)
    Q_PROPERTY(qreal radiusModal READ radiusModal CONSTANT)

public:
    using QObject::QObject;
    int fontBody() const { return 16; }
    int fontTitle() const { return 22; }
    qreal radiusModal() const { return 4; }
};

class VehicleMock : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(
        QString stateRaw READ stateRaw WRITE setStateRaw NOTIFY stateRawChanged)

public:
    using QObject::QObject;
    int state() const { return m_state; }
    QString stateRaw() const { return m_stateRaw; }

    void setState(int state)
    {
        if (m_state == state)
            return;
        m_state = state;
        emit stateChanged();
    }

    void setStateRaw(const QString &stateRaw)
    {
        if (m_stateRaw == stateRaw)
            return;
        m_stateRaw = stateRaw;
        emit stateRawChanged();
    }

signals:
    void stateChanged();
    void stateRawChanged();

private:
    int m_state = static_cast<int>(ScootEnums::VehicleState::Updating);
    QString m_stateRaw = QStringLiteral("updating");
};

class OtaMock : public QObject
{
    Q_OBJECT
    Q_PROPERTY(
        bool isActive READ isActive WRITE setIsActive NOTIFY isActiveChanged)
    Q_PROPERTY(QString dbcStatus READ dbcStatus WRITE setDbcStatus NOTIFY
                   dbcStatusChanged)
    Q_PROPERTY(int dbcDownloadProgress READ dbcDownloadProgress WRITE
                   setDbcDownloadProgress NOTIFY dbcDownloadProgressChanged)
    Q_PROPERTY(int dbcInstallProgress READ dbcInstallProgress WRITE
                   setDbcInstallProgress NOTIFY dbcInstallProgressChanged)
    Q_PROPERTY(QString dbcUpdateVersion READ dbcUpdateVersion WRITE
                   setDbcUpdateVersion NOTIFY dbcUpdateVersionChanged)

public:
    using QObject::QObject;
    bool isActive() const { return m_isActive; }
    QString dbcStatus() const { return m_dbcStatus; }
    int dbcDownloadProgress() const { return m_dbcDownloadProgress; }
    int dbcInstallProgress() const { return m_dbcInstallProgress; }
    QString dbcUpdateVersion() const { return m_dbcUpdateVersion; }

    void setIsActive(bool isActive)
    {
        if (m_isActive == isActive)
            return;
        m_isActive = isActive;
        emit isActiveChanged();
    }

    void setDbcStatus(const QString &status)
    {
        if (m_dbcStatus == status)
            return;
        m_dbcStatus = status;
        emit dbcStatusChanged();
    }

    void setDbcDownloadProgress(int progress)
    {
        if (m_dbcDownloadProgress == progress)
            return;
        m_dbcDownloadProgress = progress;
        emit dbcDownloadProgressChanged();
    }

    void setDbcInstallProgress(int progress)
    {
        if (m_dbcInstallProgress == progress)
            return;
        m_dbcInstallProgress = progress;
        emit dbcInstallProgressChanged();
    }

    void setDbcUpdateVersion(const QString &version)
    {
        if (m_dbcUpdateVersion == version)
            return;
        m_dbcUpdateVersion = version;
        emit dbcUpdateVersionChanged();
    }

signals:
    void isActiveChanged();
    void dbcStatusChanged();
    void dbcDownloadProgressChanged();
    void dbcInstallProgressChanged();
    void dbcUpdateVersionChanged();

private:
    bool m_isActive = false;
    QString m_dbcStatus = QStringLiteral("idle");
    int m_dbcDownloadProgress = 0;
    int m_dbcInstallProgress = 0;
    QString m_dbcUpdateVersion;
};

class DashboardMock : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE void setBacklightEnabled(bool enabled)
    {
        m_backlightEnabled = enabled;
    }

    bool backlightEnabled() const { return m_backlightEnabled; }

private:
    bool m_backlightEnabled = true;
};

class TranslationsMock : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString otaDownloadingUpdates READ otaDownloadingUpdates NOTIFY
                   languageChanged)
    Q_PROPERTY(QString otaPreparingUpdate READ otaPreparingUpdate NOTIFY
                   languageChanged)
    Q_PROPERTY(QString otaInstallingUpdates READ otaInstallingUpdates NOTIFY
                   languageChanged)
    Q_PROPERTY(
        QString otaPendingReboot READ otaPendingReboot NOTIFY languageChanged)
    Q_PROPERTY(
        QString otaUpdateError READ otaUpdateError NOTIFY languageChanged)
    Q_PROPERTY(
        QString otaInitializing READ otaInitializing NOTIFY languageChanged)

public:
    using QObject::QObject;

    QString otaDownloadingUpdates() const
    {
        return m_german ? QStringLiteral("Updates werden heruntergeladen...")
                        : QStringLiteral("Downloading updates...");
    }
    QString otaPreparingUpdate() const
    {
        return m_german ? QStringLiteral("Update wird vorbereitet...")
                        : QStringLiteral("Preparing update...");
    }
    QString otaInstallingUpdates() const
    {
        return m_german ? QStringLiteral("Updates werden installiert...")
                        : QStringLiteral("Installing updates...");
    }
    QString otaPendingReboot() const
    {
        return m_german ? QStringLiteral("Update installiert, wird beim "
                                         "nächsten Start angewendet")
                        : QStringLiteral(
                              "Update installed, will apply next time the "
                              "scooter is started");
    }
    QString otaUpdateError() const
    {
        return m_german ? QStringLiteral("Update-Fehler")
                        : QStringLiteral("Update error");
    }
    QString otaInitializing() const
    {
        return m_german ? QStringLiteral("Update wird initialisiert...")
                        : QStringLiteral("Initializing update...");
    }

    void setGerman(bool german)
    {
        if (m_german == german)
            return;
        m_german = german;
        emit languageChanged();
    }

signals:
    void languageChanged();

private:
    bool m_german = false;
};

class MaintenanceScreenTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void normalIdleHidesUpdateText();
    void activeUpdateAndErrorKeepDynamicText();
    void pendingRebootBalancesEnglishAndGerman();
    void connectionInfoKeepsCopyAndCenteredBounds();

private:
    QObject *textItem(const char *objectName) const;
    void verifyBalancedText(QObject *item, qreal maxWidth,
                            bool expectWrapped) const;

    ThemeMock m_theme;
    VehicleMock m_vehicle;
    OtaMock m_ota;
    DashboardMock m_dashboard;
    TranslationsMock m_translations;
    QQuickView *m_view = nullptr;
};

void MaintenanceScreenTest::initTestCase()
{
    qmlRegisterUncreatableMetaObject(ScootEnums::staticMetaObject, "ScootUI", 1,
                                     0, "Scooter", QString());
}

void MaintenanceScreenTest::init()
{
    m_ota.setIsActive(false);
    m_ota.setDbcStatus(QStringLiteral("idle"));
    m_ota.setDbcDownloadProgress(0);
    m_ota.setDbcInstallProgress(0);
    m_ota.setDbcUpdateVersion(QString());
    m_translations.setGerman(false);

    m_view = new QQuickView;
    m_view->setResizeMode(QQuickView::SizeRootObjectToView);
    m_view->resize(480, 480);
    QQmlContext *context = m_view->rootContext();
    context->setContextProperty(QStringLiteral("themeStore"), &m_theme);
    context->setContextProperty(QStringLiteral("vehicleStore"), &m_vehicle);
    context->setContextProperty(QStringLiteral("otaStore"), &m_ota);
    context->setContextProperty(QStringLiteral("dashboardStore"), &m_dashboard);
    context->setContextProperty(QStringLiteral("translations"),
                                &m_translations);
    m_view->setSource(QUrl::fromLocalFile(QStringLiteral(
        SCOOTUI_SOURCE_DIR "/qml/screens/MaintenanceScreen.qml")));

    const QString errors = [&]()
    {
        QStringList messages;
        for (const QQmlError &error : m_view->errors())
            messages.append(error.toString());
        return messages.join(QLatin1Char('\n'));
    }();
    QVERIFY2(m_view->status() == QQuickView::Ready, qPrintable(errors));
    m_view->show();
    QCoreApplication::processEvents();
}

void MaintenanceScreenTest::cleanup()
{
    delete m_view;
    m_view = nullptr;
}

QObject *MaintenanceScreenTest::textItem(const char *objectName) const
{
    QObject *item = m_view->rootObject()->findChild<QObject *>(
        QString::fromLatin1(objectName));
    if (!item)
        QTest::qFail("Named text item was not found", __FILE__, __LINE__);
    return item;
}

void MaintenanceScreenTest::verifyBalancedText(QObject *item, qreal maxWidth,
                                               bool expectWrapped) const
{
    QVERIFY(item);
    QVERIFY2(item->property("maxWidth").isValid(),
             "Wrapped text must expose BalancedText.maxWidth");
    QCOMPARE(item->property("maxWidth").toReal(), maxWidth);
    QVERIFY(item->property("width").toReal() <= maxWidth + 0.5);
    if (expectWrapped)
        QVERIFY(item->property("lineCount").toInt() > 1);
}

void MaintenanceScreenTest::normalIdleHidesUpdateText()
{
    QObject *status = textItem("otaStatusText");
    QCOMPARE(status->property("text").toString(),
             QStringLiteral("Initializing update..."));
    QVERIFY(!status->property("visible").toBool());
    verifyBalancedText(status, 416, false);
}

void MaintenanceScreenTest::activeUpdateAndErrorKeepDynamicText()
{
    QObject *status = textItem("otaStatusText");
    m_ota.setIsActive(true);
    m_ota.setDbcStatus(QStringLiteral("downloading"));
    QTRY_VERIFY(status->property("visible").toBool());
    QTRY_COMPARE(status->property("text").toString(),
                 QStringLiteral("Downloading updates..."));
    verifyBalancedText(status, 416, false);

    m_ota.setDbcStatus(QStringLiteral("error"));
    QTRY_COMPARE(status->property("text").toString(),
                 QStringLiteral("Update error"));
    QCOMPARE(status->property("lineCount").toInt(), 1);
}

void MaintenanceScreenTest::pendingRebootBalancesEnglishAndGerman()
{
    QObject *status = textItem("otaStatusText");
    m_ota.setIsActive(true);
    m_ota.setDbcStatus(QStringLiteral("pending-reboot"));
    QTRY_COMPARE(
        status->property("text").toString(),
        QStringLiteral(
            "Update installed, will apply next time the scooter is started"));
    verifyBalancedText(status, 416, true);
    const qreal englishWidth = status->property("width").toReal();
    QVERIFY(englishWidth < 416);

    m_translations.setGerman(true);
    QTRY_COMPARE(
        status->property("text").toString(),
        QStringLiteral(
            "Update installiert, wird beim nächsten Start angewendet"));
    verifyBalancedText(status, 416, true);
    QVERIFY(status->property("width").toReal() < 416);
}

void MaintenanceScreenTest::connectionInfoKeepsCopyAndCenteredBounds()
{
    m_view->rootObject()->setProperty("showConnectionInfo", true);
    QCoreApplication::processEvents();

    QObject *title = textItem("connectionTitle");
    QObject *details = textItem("connectionDetails");
    QObject *overrideHint = textItem("connectionOverrideHint");

    QCOMPARE(title->property("text").toString(),
             QStringLiteral("Trying to connect to vehicle system..."));
    QCOMPARE(
        details->property("text").toString(),
        QStringLiteral(
            "This usually indicates a missing or unreliable connection between "
            "the dashboard computer (DBC) and the middle driver board "
            "(MDB).\n\n"
            "Check the USB cable if this persists."));
    QCOMPARE(
        overrideHint->property("text").toString(),
        QStringLiteral(
            "To put your scooter into drive mode anyway, raise the kickstand, "
            "hold both brakes and press the seatbox button."));

    verifyBalancedText(title, 416, false);
    verifyBalancedText(details, 416, true);
    verifyBalancedText(overrideHint, 416, true);
    QCOMPARE(details->property("lineHeight").toReal(), 1.4);
    QCOMPARE(overrideHint->property("lineHeight").toReal(), 1.4);

    auto *rootItem = qobject_cast<QQuickItem *>(m_view->rootObject());
    QVERIFY(rootItem);
    for (QObject *item : {title, details, overrideHint})
    {
        auto *quickItem = qobject_cast<QQuickItem *>(item);
        QVERIFY(quickItem);
        QQuickItem *layoutCell = quickItem->parentItem();
        QVERIFY(layoutCell);
        QCOMPARE(layoutCell->width(), 416);
        const QPointF center = quickItem->mapToItem(
            rootItem, QPointF(quickItem->width() / 2, quickItem->height() / 2));
        const QString message = QStringLiteral("%1 center=%2 width=%3 cellX=%4")
                                    .arg(item->objectName())
                                    .arg(center.x())
                                    .arg(quickItem->width())
                                    .arg(layoutCell->x());
        QVERIFY2(qAbs(center.x() - 240) <= 0.5, qPrintable(message));
    }
}

QTEST_MAIN(MaintenanceScreenTest)
#include "MaintenanceScreenTest.moc"

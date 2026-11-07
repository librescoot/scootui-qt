#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <pwd.h>
#include <unistd.h>
#include "services/DoctorService.h"

class DoctorServiceTest : public QObject
{
    Q_OBJECT
    static QByteArray report(const QString &verdict = "ok")
    {
        QJsonArray checks;
        for (const auto *name : {"link", "kernel", "services", "ota", "faults", "storage"})
            checks.append(QJsonObject{{"name", name}, {"verdict", verdict}, {"detail", "Diagnostic detail"}, {"hint", "Read the logs"}});
        return QJsonDocument(QJsonObject{{"schema_version", 1}, {"checks", checks}}).toJson(QJsonDocument::Compact);
    }
    static QString script(QTemporaryDir &directory, const QString &name, const QByteArray &content)
    {
        const auto path = directory.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return {};
        file.write(content);
        return path;
    }
    static QVariantMap board(const DoctorService &service, int index) { return service.boards().at(index).toMap(); }
private slots:
    void validateReports()
    {
        QVariantList checks;
        QString error;
        QVERIFY(DoctorService::decodeReport(report(), checks, error));
        QCOMPARE(checks.size(), 6);
        for (const auto *verdict : {"ok", "warning", "attention", "unknown"})
            QVERIFY(DoctorService::decodeReport(report(verdict), checks, error));
        for (const auto &data : {QByteArray{}, QByteArray("[]"), QByteArray("garbage"), report("bogus"), QByteArray(300000, 'x')}) {
            QVERIFY(!DoctorService::decodeReport(data, checks, error));
            QVERIFY(checks.isEmpty());
            QVERIFY(!error.isEmpty());
        }
        auto object = QJsonDocument::fromJson(report()).object();
        auto ordered = object["checks"].toArray();
        auto service = ordered[2].toObject();
        service["verdict"] = "attention";
        ordered[2] = service;
        object["checks"] = ordered;
        QVERIFY(DoctorService::decodeReport(QJsonDocument(object).toJson(), checks, error));
        QCOMPARE(checks.first().toMap().value("name").toString(), QStringLiteral("services"));
        object.remove("schema_version");
        QVERIFY(!DoctorService::decodeReport(QJsonDocument(object).toJson(), checks, error));
        QCOMPARE(error, QStringLiteral("unsupported"));
        object["schema_version"] = 1;
        auto entries = object["checks"].toArray();
        entries[1] = entries[0];
        object["checks"] = entries;
        QVERIFY(!DoctorService::decodeReport(QJsonDocument(object).toJson(), checks, error));
        entries.removeLast();
        object["checks"] = entries;
        QVERIFY(!DoctorService::decodeReport(QJsonDocument(object).toJson(), checks, error));
    }

    void nonzeroReportAndIndependentFailure()
    {
        QTemporaryDir directory;
        const auto good = script(directory, "report.sh", "printf '%s' '" + report("attention") + "'\nexit 1\n");
        DoctorService service({"/bin/sh", {good}}, {"/definitely-missing-doctor", {}}, 2000);
        service.start();
        QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 3000);
        QCOMPARE(board(service, 0).value("state").toString(), QStringLiteral("done"));
        QCOMPARE(board(service, 0).value("attention").toInt(), 6);
        QCOMPARE(board(service, 1).value("state").toString(), QStringLiteral("error"));
        QCOMPARE(board(service, 1).value("error").toString(), QStringLiteral("unavailable"));
    }

    void timeoutCancellationAndRetry()
    {
        QTemporaryDir directory;
        const auto delayed = script(directory, "delayed.sh", "sleep 0.3\nprintf '%s' '" + report() + "'\n");
        const auto good = script(directory, "good.sh", "printf '%s' '" + report() + "'\n");
        DoctorService service({"/bin/sh", {delayed}}, {"/bin/sh", {good}}, 100);
        service.start();
        service.start();
        QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 1000);
        QCOMPARE(board(service, 0).value("error").toString(), QStringLiteral("timeout"));
        QCOMPARE(board(service, 1).value("state").toString(), QStringLiteral("done"));
        service.start();
        service.cancel();
        QVERIFY(!service.running());
        QCOMPARE(board(service, 0).value("error").toString(), QStringLiteral("cancelled"));
        script(directory, "delayed.sh", "printf '%s' '" + report("warning") + "'\n");
        service.start();
        QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 1000);
        QTest::qWait(400);
        QCOMPARE(board(service, 0).value("warnings").toInt(), 6);
        QCOMPARE(board(service, 0).value("state").toString(), QStringLiteral("done"));
    }

    void remoteCommandUsesAccountHome()
    {
        const auto *account = getpwuid(geteuid());
        QVERIFY(account);
        const QByteArray accountHome(account->pw_dir);
        QTemporaryDir directory;
        const auto good = script(directory, "report.sh",
            "test \"$HOME\" = \"$EXPECTED_HOME\" || exit 255\nprintf '%s' '" + report() + "'\n");
        const auto inherited = qgetenv("HOME");
        const auto expected = qgetenv("EXPECTED_HOME");
        const bool homeWasSet = qEnvironmentVariableIsSet("HOME");
        const bool expectedWasSet = qEnvironmentVariableIsSet("EXPECTED_HOME");
        qputenv("HOME", directory.path().toLocal8Bit());
        qputenv("EXPECTED_HOME", accountHome);
        DoctorService service({"/bin/sh", {"-c", "test \"$HOME\" != \"$EXPECTED_HOME\" && printf '%s' '" + report() + "'"}},
                              {"/bin/sh", {good}}, 2000);
        service.start();
        if (homeWasSet) qputenv("HOME", inherited); else qunsetenv("HOME");
        if (expectedWasSet) qputenv("EXPECTED_HOME", expected); else qunsetenv("EXPECTED_HOME");
        QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 3000);
        QCOMPARE(board(service, 0).value("state").toString(), QStringLiteral("done"));
        QCOMPARE(board(service, 1).value("state").toString(), QStringLiteral("done"));
        QCOMPARE(board(service, 1).value("checks").toList().size(), 6);
    }

    void excessiveOutputStopsBothChannels()
    {
        DoctorService service({"/bin/sh", {"-c", "printf '%270000s' ''"}},
                              {"/bin/sh", {"-c", "printf '%270000s' '' >&2"}}, 2000);
        service.start();
        QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 3000);
        for (int i = 0; i < 2; ++i) {
            QCOMPARE(board(service, i).value("error").toString(), QStringLiteral("too-large"));
            QVERIFY(board(service, i).value("checks").toList().isEmpty());
        }
    }

    void transportFailureCannotValidateJson()
    {
        QTemporaryDir directory;
        const auto failed = script(directory, "ssh.sh", "printf '%s' '" + report() + "'\nexit 255\n");
        const auto invalid = script(directory, "invalid.sh", "printf 'not-json'\n");
        DoctorService service({"/bin/sh", {failed}}, {"/bin/sh", {invalid}}, 2000);
        service.start();
        QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 3000);
        QCOMPARE(board(service, 0).value("error").toString(), QStringLiteral("unavailable"));
        QCOMPARE(board(service, 1).value("error").toString(), QStringLiteral("invalid"));
    }
};

QTEST_GUILESS_MAIN(DoctorServiceTest)
#include "DoctorServiceTest.moc"

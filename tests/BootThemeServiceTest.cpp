#include <QtTest>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>

#include "repositories/InMemoryMdbRepository.h"
#include "services/BootThemeService.h"

// The parts of the boot-theme service that do not need a U-Boot environment:
// what counts as a selectable theme, how a stored sound value is read, and the
// theme-to-file mapping the launcher and the sound test must agree on.
class BootThemeServiceTest : public QObject
{
    Q_OBJECT
private slots:
    void themesInDirFiltersAndOrders();
    void themesInDirReportsMissingDirectory();
    void parseSoundValueAcceptsBothSpellings();
    void soundFileNameMatchesTheLauncher();
    void displayNameFallsBackForUnknownThemes();
    void requestsGoThroughTheSharedInterface();
    void publishedStateDrivesThemeAndSound();
    void refusalIsReported();
    void staleRefusalIsIgnored();

private:
    QString makeThemeDir();
};

QString BootThemeServiceTest::makeThemeDir()
{
    const QString dir = QDir::tempPath() + QStringLiteral("/bootthemetest-")
                        + QString::number(QCoreApplication::applicationPid());
    QDir().remove(dir);
    QDir().mkpath(dir);
    return dir;
}

void BootThemeServiceTest::themesInDirFiltersAndOrders()
{
    const QString dir = makeThemeDir();
    QVERIFY(QDir(dir).exists());

    const QStringList names = {
        QStringLiteral("coopertino"), QStringLiteral("windowsxp"),
        QStringLiteral("librescoot"), QStringLiteral("zeta"),
        QStringLiteral("alpha"),
    };
    for (const QString &name : names) {
        QFile file(dir + QLatin1Char('/') + name + QStringLiteral(".json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{}");
    }
    // Not themes: the packed stream, and a name fw_setenv could not be trusted
    // with. Neither may appear in the cycle.
    QFile stream(dir + QStringLiteral("/librescoot.lsba"));
    QVERIFY(stream.open(QIODevice::WriteOnly));
    stream.write("x");
    stream.close();
    QFile bad(dir + QStringLiteral("/not a theme.json"));
    QVERIFY(bad.open(QIODevice::WriteOnly));
    bad.write("{}");
    bad.close();
    // A symlink alias is a theme under its own name, which is what librescoot-xp
    // is on the device.
    QVERIFY(QFile::link(dir + QStringLiteral("/windowsxp.json"),
                        dir + QStringLiteral("/librescoot-xp.json")));

    bool readable = false;
    const QStringList themes = BootThemeService::themesInDir(dir, &readable);
    QVERIFY(readable);
    QCOMPARE(themes, QStringList({QStringLiteral("librescoot"), QStringLiteral("windowsxp"),
                                  QStringLiteral("librescoot-xp"), QStringLiteral("coopertino"),
                                  QStringLiteral("alpha"), QStringLiteral("zeta")}));

    QDir(dir).removeRecursively();
}

void BootThemeServiceTest::themesInDirReportsMissingDirectory()
{
    bool readable = true;
    const QStringList themes = BootThemeService::themesInDir(
        QDir::tempPath() + QStringLiteral("/bootthemetest-absent"), &readable);
    QVERIFY(!readable);
    QVERIFY(themes.isEmpty());
}

void BootThemeServiceTest::parseSoundValueAcceptsBothSpellings()
{
    bool ok = false;

    QVERIFY(BootThemeService::parseSoundValue(QString(), &ok));
    QVERIFY(ok);   // unset means on
    QVERIFY(BootThemeService::parseSoundValue(QStringLiteral("1"), &ok));
    QVERIFY(ok);
    QVERIFY(BootThemeService::parseSoundValue(QStringLiteral("ON"), &ok));
    QVERIFY(ok);
    QVERIFY(!BootThemeService::parseSoundValue(QStringLiteral("0"), &ok));
    QVERIFY(ok);
    QVERIFY(!BootThemeService::parseSoundValue(QStringLiteral(" off "), &ok));
    QVERIFY(ok);
    // Unrecognised keeps the historical behaviour (sound on) but says so.
    QVERIFY(BootThemeService::parseSoundValue(QStringLiteral("maybe"), &ok));
    QVERIFY(!ok);
}

void BootThemeServiceTest::soundFileNameMatchesTheLauncher()
{
    BootThemeService service;

    QCOMPARE(service.soundFileName(QStringLiteral("librescoot")),
             QStringLiteral("scooter-unlock.wav"));
    QCOMPARE(service.soundFileName(QStringLiteral("windowsxp")),
             QStringLiteral("windowsxp.wav"));
    QCOMPARE(service.soundFileName(QStringLiteral("librescoot-xp")),
             QStringLiteral("librescoot-xp.wav"));
    QCOMPARE(service.soundFileName(QStringLiteral("coopertino")),
             QStringLiteral("coopertino.wav"));
    QVERIFY(service.soundFileName(QStringLiteral("nope")).isEmpty());
}

void BootThemeServiceTest::displayNameFallsBackForUnknownThemes()
{
    BootThemeService service;

    QCOMPARE(service.displayName(QStringLiteral("windowsxp")), QStringLiteral("Windows XP"));
    QCOMPARE(service.displayName(QStringLiteral("librescoot-xp")), QStringLiteral("Librescoot XP"));
    QCOMPARE(service.displayName(QStringLiteral("my_theme-2")), QStringLiteral("My Theme 2"));
}

// The shared interface with lsc and dbc-dispatcher: the request goes to
// boot:desired and is announced on boot:command. Nothing here writes the
// U-Boot environment, which is what keeps one writer for it.
void BootThemeServiceTest::requestsGoThroughTheSharedInterface()
{
    InMemoryMdbRepository repo;
    BootThemeService service;
    service.setRepository(&repo);

    QStringList commands;
    repo.subscribe(QStringLiteral("boot:command"),
                   [&commands](const QString &, const QString &message) {
                       commands.append(message);
                   });

    QVERIFY(service.setTheme(QStringLiteral("coopertino")));
    QCOMPARE(repo.getAll(QStringLiteral("boot:desired")).value(QStringLiteral("theme")),
             QStringLiteral("coopertino"));
    QCOMPARE(commands.last(), QStringLiteral("boot-theme coopertino"));

    QVERIFY(service.setSoundEnabled(false));
    QCOMPARE(repo.getAll(QStringLiteral("boot:desired")).value(QStringLiteral("sound")),
             QStringLiteral("0"));
    QCOMPARE(commands.last(), QStringLiteral("boot-sound 0"));

    // Shapes the DBC would have to defend against are refused here instead.
    QVERIFY(!service.setTheme(QStringLiteral("bad name")));
    QVERIFY(!service.setTheme(QString()));
    QVERIFY(!service.setTheme(QStringLiteral("../escape")));
}

// Reads come from what the DBC published, not from what was requested.
void BootThemeServiceTest::publishedStateDrivesThemeAndSound()
{
    InMemoryMdbRepository repo;
    BootThemeService service;
    service.setRepository(&repo);

    QSignalSpy themeSpy(&service, &BootThemeService::themeChanged);
    QSignalSpy soundSpy(&service, &BootThemeService::soundChanged);

    repo.set(QStringLiteral("boot:dbc"), QStringLiteral("theme"), QStringLiteral("coopertino"), true);
    repo.set(QStringLiteral("boot:dbc"), QStringLiteral("sound"), QStringLiteral("0"), true);

    QCOMPARE(service.theme(), QStringLiteral("coopertino"));
    QCOMPARE(service.soundEnabled(), false);
    QCOMPARE(themeSpy.count(), 1);
    QCOMPARE(soundSpy.count(), 1);
}

void BootThemeServiceTest::refusalIsReported()
{
    InMemoryMdbRepository repo;
    BootThemeService service;
    service.setRepository(&repo);
    QSignalSpy failSpy(&service, &BootThemeService::applyFailed);

    QVERIFY(service.setTheme(QStringLiteral("nope")));

    // The DBC answers, with a timestamp newer than the request, refusing it.
    const QString answeredAt = QString::number(QDateTime::currentSecsSinceEpoch() + 60);
    repo.set(QStringLiteral("boot:dbc"), QStringLiteral("theme"), QStringLiteral("librescoot"), false);
    repo.set(QStringLiteral("boot:dbc"), QStringLiteral("note"), QStringLiteral("unknown-theme"), false);
    repo.set(QStringLiteral("boot:dbc"), QStringLiteral("updated"), answeredAt, false);
    repo.requestAll(QStringLiteral("boot:dbc"));

    QCOMPARE(failSpy.count(), 1);
    QVERIFY(failSpy.first().first().toString().contains(QStringLiteral("not installed")));
}

// A refusal left in the published state by an earlier attempt must not be read
// as the verdict on a request made afterwards.
void BootThemeServiceTest::staleRefusalIsIgnored()
{
    InMemoryMdbRepository repo;
    BootThemeService service;
    service.setRepository(&repo);

    const QString earlier = QString::number(QDateTime::currentSecsSinceEpoch() - 600);
    repo.set(QStringLiteral("boot:dbc"), QStringLiteral("theme"), QStringLiteral("librescoot"), false);
    repo.set(QStringLiteral("boot:dbc"), QStringLiteral("note"), QStringLiteral("unknown-theme"), false);
    repo.set(QStringLiteral("boot:dbc"), QStringLiteral("updated"), earlier, false);
    repo.requestAll(QStringLiteral("boot:dbc"));

    QSignalSpy failSpy(&service, &BootThemeService::applyFailed);
    QVERIFY(service.setTheme(QStringLiteral("coopertino")));
    QCOMPARE(failSpy.count(), 0);
}

QTEST_MAIN(BootThemeServiceTest)
#include "BootThemeServiceTest.moc"

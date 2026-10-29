#include <QtTest>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/BootTransition.h"

class BootTransitionTest : public QObject
{
    Q_OBJECT
private:
    static QString script(const QTemporaryDir &dir, const QByteArray &body)
    {
        const QString path = dir.filePath(QStringLiteral("helper"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write("#!/bin/sh\n" + body + "\n");
        file.close();
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }

    static QByteArray contents(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }

private slots:
    void successfulFadeDoesNotRevealEarly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString alpha = dir.filePath(QStringLiteral("alpha"));
        QFile file(alpha);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("0\n");
        file.close();
        const QString helper = script(dir,
            "test \"$*\" = 'fade 0 255 25' || exit 2\n"
            "sleep 0.05\nprintf '255\\n' > \"$(dirname \"$0\")/alpha\"");
        BootTransition transition(nullptr, alpha, helper, 25);
        QSignalSpy finished(&transition, &BootTransition::finished);
        transition.start();
        transition.start();
        QCOMPARE(contents(alpha), QByteArray("0\n"));
        QCOMPARE(finished.size(), 0);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), true);
        QCOMPARE(contents(alpha), QByteArray("255\n"));
    }

    void helperFailureRevealsDirectly_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("exit failure") << QByteArray("exit 7");
        QTest::newRow("crash") << QByteArray("kill -KILL $$");
        QTest::newRow("timeout") << QByteArray("exec sleep 5");
        QTest::newRow("missing helper") << QByteArray();
    }

    void helperFailureRevealsDirectly()
    {
        QFETCH(QByteArray, body);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString alpha = dir.filePath(QStringLiteral("alpha"));
        const QString helper = body.isEmpty()
            ? dir.filePath(QStringLiteral("absent")) : script(dir, body);
        BootTransition transition(nullptr, alpha, helper, 1);
        QSignalSpy finished(&transition, &BootTransition::finished);
        transition.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 2500);
        QCOMPARE(finished.first().first().toBool(), true);
        QCOMPARE(contents(alpha), QByteArray("255\n"));
        QTest::qWait(50);
        QCOMPARE(finished.size(), 1);
    }

    void revealFailureDoesNotReportVisible()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        BootTransition transition(nullptr,
            dir.filePath(QStringLiteral("missing-directory/alpha")),
            script(dir, "exit 7"), 1);
        QSignalSpy finished(&transition, &BootTransition::finished);
        transition.start();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), false);
    }
};

QTEST_GUILESS_MAIN(BootTransitionTest)
#include "BootTransitionTest.moc"

#include <QtTest>
#include <QTemporaryDir>

#include "core/DataPartition.h"

class DataPartitionTest : public QObject
{
    Q_OBJECT

private slots:
    void mountedFilesystemIsMountPoint();
    void plainDirectoryIsNotMountPoint();
    void missingPathIsNotMountPoint();
    void refreshEmitsOnceOnEdge();
};

void DataPartitionTest::mountedFilesystemIsMountPoint()
{
#ifdef Q_OS_LINUX
    QVERIFY(DataPartition::isMountPoint(QStringLiteral("/proc")));
#else
    QSKIP("needs a known mount point");
#endif
}

void DataPartitionTest::plainDirectoryIsNotMountPoint()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("data")));
    QVERIFY(!DataPartition::isMountPoint(dir.path() + QStringLiteral("/data")));
}

void DataPartitionTest::missingPathIsNotMountPoint()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!DataPartition::isMountPoint(dir.path() + QStringLiteral("/absent")));
}

void DataPartitionTest::refreshEmitsOnceOnEdge()
{
    DataPartition partition;
    QSignalSpy spy(&partition, &DataPartition::becameMounted);
    const bool initially = partition.mounted();
    partition.refresh();
    partition.refresh();
    if (initially) {
        QCOMPARE(spy.count(), 0);
    } else {
        QCOMPARE(spy.count(), partition.mounted() ? 1 : 0);
    }
}

QTEST_GUILESS_MAIN(DataPartitionTest)
#include "DataPartitionTest.moc"

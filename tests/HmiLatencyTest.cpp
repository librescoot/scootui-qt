#include <QtTest>
#include <QQuickWindow>
#include <QThread>
#include "core/HmiLatencyMonitor.h"
#include "core/HmiLatencyMetrics.h"

class HmiLatencyTest : public QObject
{
    Q_OBJECT
private slots:
    void windowLifecycleDoesNotRequestFrames()
    {
        QQuickWindow window;
        QSignalSpy swaps(&window, &QQuickWindow::frameSwapped);
        auto monitor = std::make_unique<HmiLatencyMonitor>();
        monitor->attachWindow(&window);
        monitor->attachWindow(&window); // idempotent attachment
        QTest::qWait(20);
        QCOMPARE(swaps.count(), 0);
        // Exercise the direct render-thread callbacks without depending on a GPU.
        auto render = QThread::create([&window]() {
            QMetaObject::invokeMethod(&window, "beforeSynchronizing", Qt::DirectConnection);
            QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection);
        });
        render->start();
        QVERIFY(render->wait(2000));
        delete render;
        monitor->stop();
        monitor.reset();
        QVERIFY(QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection));
        auto second = std::make_unique<HmiLatencyMonitor>();
        auto temporary = std::make_unique<QQuickWindow>();
        second->attachWindow(temporary.get());
        temporary.reset();
        second->stop();
    }
    void guiLatenessThresholdAndCollapsedTimeouts()
    {
        HmiGuiLatencyMetrics metrics;
        metrics.start(1000);
        QCOMPARE(metrics.tick(1100), qint64(0));
        QCOMPARE(metrics.tick(1249), qint64(49));
        QCOMPARE(metrics.lateTicks, quint64(0));
        QCOMPARE(metrics.tick(1399), qint64(50));
        QCOMPARE(metrics.lateTicks, quint64(1));
        QCOMPARE(metrics.tick(5000), qint64(3501));
        QCOMPARE(metrics.maxLate, qint64(3501));
        QCOMPARE(metrics.tick(5100), qint64(0));
        QCOMPARE(metrics.lateTicks, quint64(2));
        QVERIFY(!metrics.reportDue(10999));
        QVERIFY(metrics.reportDue(11000));
    }
    void idleSwapGapIsNotFrameWork()
    {
        HmiFrameMetrics metrics;
        metrics.began(1000);
        metrics.swapped(1010);
        metrics.began(20000);
        metrics.swapped(20010);
        QCOMPARE(metrics.maxSwapGap.load(), qint64(19000));
        QCOMPARE(metrics.maxFrameWork.load(), qint64(10));
        QCOMPARE(metrics.swaps.load(), quint64(2));
    }
    void frameWorkAndExposureReset()
    {
        HmiFrameMetrics metrics;
        metrics.began(1000);
        metrics.swapped(1200);
        QCOMPARE(metrics.maxFrameWork.load(), qint64(200));
        QCOMPARE(metrics.beganAt.load(), qint64(0));
        metrics.began(1300);
        metrics.resetInterval(); // hidden/unexposed intervals must not carry frame history
        metrics.began(10000);
        metrics.swapped(10010);
        QCOMPARE(metrics.maxSwapGap.load(), qint64(0));
        QCOMPARE(metrics.maxFrameWork.load(), qint64(200));
        QCOMPARE(metrics.maxFrameWork.exchange(0), qint64(200));
        QCOMPARE(metrics.maxFrameWork.load(), qint64(0));
    }
};
QTEST_MAIN(HmiLatencyTest)
#include "HmiLatencyTest.moc"

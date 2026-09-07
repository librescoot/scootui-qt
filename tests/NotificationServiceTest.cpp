#include <QtTest>
#include "services/AttentionPolicy.h"
#include "services/NotificationService.h"

class NotificationServiceTest : public QObject
{
    Q_OBJECT
private slots:
    void priorityAndCompanion()
    {
        QList<QVariantMap> conditions;
        QVariantMap warning{{"id", "warning"}, {"priority", 2}, {"kind", "warning"}};
        conditions.append(warning);
        QVariantMap nav{{"id", "navigation-session"}, {"kind", "nav"}, {"valid", true}, {"distance", 800.0}};
        const auto selected = AttentionPolicy::select(conditions, {}, nav, true, 0);
        QCOMPARE(selected.main.value("id").toString(), QStringLiteral("warning"));
        QCOMPARE(selected.companion.value("id").toString(), QStringLiteral("navigation-session"));
        QCOMPARE(selected.height, 92);
    }

    void imminentNavigationWins()
    {
        QVariantMap warning{{"id", "warning"}, {"priority", 2}, {"kind", "warning"}};
        QVariantMap nav{{"id", "navigation-session"}, {"kind", "nav"}, {"valid", true},
                        {"status", 2}, {"distance", 40.0}};
        const auto selected = AttentionPolicy::select({warning}, {}, nav, true, 0);
        QCOMPARE(selected.main.value("id").toString(), QStringLiteral("navigation-session"));
        QVERIFY(selected.companion.isEmpty());
        QCOMPARE(selected.height, 96);
    }

    void navigationErrorCannotCompanion()
    {
        QVariantMap critical{{"id", "critical"}, {"priority", 0}, {"kind", "critical"}};
        QVariantMap errorNav{{"id", "navigation-session"}, {"kind", "nav"}, {"valid", true},
                             {"status", 5}, {"distance", 20.0}};
        const auto selected = AttentionPolicy::select({critical}, {}, errorNav, true, 0);
        QCOMPARE(selected.main.value("id").toString(), QStringLiteral("critical"));
        QVERIFY(selected.companion.isEmpty());
    }

    void distantNavigationUsesCompactRow()
    {
        QVariantMap nav{{"id", "navigation-session"}, {"kind", "nav"}, {"valid", true},
                        {"status", 2}, {"distance", 800.0}};
        const auto selected = AttentionPolicy::select({}, {}, nav, true, 0);
        QCOMPARE(selected.main.value("id").toString(), QStringLiteral("navigation-session"));
        QCOMPARE(selected.height, 56);
    }

    void criticalPreemptsAndDoesNotExpire()
    {
        NotificationService service(true);
        QSignalSpy conditionSpy(&service, &NotificationService::conditionPresented);
        service.publishCondition("battery-fault-0", "battery", "Battery", "Stop", 0, "critical");
        service.publishCondition("redis-disconnect", "connection", "Connection", "Lost", 0, "critical");
        QCOMPARE(service.presentation().value("main").toMap().value("id").toString(), QStringLiteral("battery-fault-0"));
        QVERIFY(service.telemetryTrustLost());
        QCOMPARE(conditionSpy.count(), 1);
        service.publishCondition("battery-fault-0", "battery", "Battery changed", "Still stop", 0, "critical");
        QCOMPARE(conditionSpy.count(), 1);
        QTest::qWait(600);
        QVERIFY(service.active().size() == 2);
        QCOMPARE(service.presentation().value("main").toMap().value("id").toString(), QStringLiteral("battery-fault-0"));
        service.resolveCondition("battery-fault-0");
        service.resolveCondition("redis-disconnect");
        QVERIFY(service.active().isEmpty());
        QVERIFY(!service.telemetryTrustLost());
    }

    void conditionCueOnlyFollowsPresentationTransitions()
    {
        NotificationService service(true);
        QSignalSpy conditionSpy(&service, &NotificationService::conditionPresented);
        service.publishCondition("first", "test", "First", {}, 0, "critical");
        service.publishCondition("hidden", "test", "Hidden", {}, 2, "warning");
        QCOMPARE(conditionSpy.count(), 1);
        service.publishCondition("hidden", "test", "Escalated", {}, 0, "critical");
        QCOMPARE(conditionSpy.count(), 1);
        service.resolveCondition("first");
        QCOMPARE(conditionSpy.count(), 2);
        service.publishCondition("hidden", "test", "Updated", {}, 0, "critical");
        QCOMPARE(conditionSpy.count(), 2);
        service.resolveCondition("hidden");
        service.publishCondition("visible", "test", "Visible", {}, 2, "warning");
        QCOMPARE(conditionSpy.count(), 3);
        service.publishCondition("visible", "test", "Escalated", {}, 0, "critical");
        QCOMPARE(conditionSpy.count(), 4);
        service.publishCondition("visible", "test", "Updated", {}, 0, "critical");
        QCOMPARE(conditionSpy.count(), 4);
    }

    void transientExpiresAndHistoryIsBounded()
    {
        NotificationService service(true);
        service.publishEvent("one", "test", "One", "Body", 4, "info", 10);
        QTest::qWait(300);
        QVERIFY(service.active().isEmpty());
        QCOMPARE(service.history().size(), 1);
    }

    void queueEvictsLowestPriorityFirst()
    {
        NotificationService service(true);
        service.publishEvent("warning", "test", "Warning", "Keep", 2, "warning", 1000);
        for (int i = 0; i < 20; ++i)
            service.publishEvent(QStringLiteral("low-%1").arg(i), "test", "Low", {}, 4, "info", 10);
        QTest::qWait(300);
        QVERIFY(service.history().size() < 20);
        QCOMPARE(service.presentation().value("main").toMap().value("id").toString(), QStringLiteral("warning"));
    }

    void mapUpdateRemainsSilent()
    {
        NotificationService service(true);
        QSignalSpy conditionSpy(&service, &NotificationService::conditionPresented);
        service.setMapUpdateAvailable(true, "Update available");
        QCOMPARE(conditionSpy.count(), 0);
        service.publishCondition("warning", "test", "Warning", {}, 2, "warning");
        QCOMPARE(conditionSpy.count(), 1);
        service.resolveCondition("warning");
        QCOMPARE(conditionSpy.count(), 1);
        service.setMapUpdateAvailable(false);
        service.setMapUpdateAvailable(true, "Update available");
        QCOMPARE(conditionSpy.count(), 1);
        QCOMPARE(service.presentation().value("main").toMap().value("id").toString(),
                 QStringLiteral("map-update"));
    }

    void coverageAndMapUpdateUseSharedPolicy()
    {
        NotificationService service(true);
        service.setCoverageWarning(true);
        QVERIFY(service.coverageWarning());
        QVERIFY(service.presentation().value("main").toMap().isEmpty());
        service.setSurface("map");
        QCOMPARE(service.presentation().value("main").toMap().value("id").toString(), QStringLiteral("map-coverage"));
        service.setSurface("cluster");
        service.setMapUpdateAvailable(true, "Map update");
        QCOMPARE(service.presentation().value("main").toMap().value("id").toString(), QStringLiteral("map-update"));
    }

    void simulatorInjectionIsLocalOnly()
    {
        NotificationService simulatorService(true);
        simulatorService.simulateError("Error", "Only local");
        simulatorService.simulateCritical("Test", "Only local");
        QVERIFY(simulatorService.active().size() == 1);
        simulatorService.clearSimulatorEntries();
        QVERIFY(simulatorService.active().isEmpty());

        NotificationService productionService(false);
        productionService.simulateError("Error", "Ignored");
        productionService.simulateCritical("Critical", "Ignored");
        QVERIFY(productionService.active().isEmpty());
    }
};

QTEST_MAIN(NotificationServiceTest)
#include "NotificationServiceTest.moc"

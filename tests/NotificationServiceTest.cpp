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
    }

    void imminentNavigationWins()
    {
        QVariantMap warning{{"id", "warning"}, {"priority", 2}, {"kind", "warning"}};
        QVariantMap nav{{"id", "navigation-session"}, {"kind", "nav"}, {"valid", true},
                        {"status", 2}, {"distance", 40.0}};
        const auto selected = AttentionPolicy::select({warning}, {}, nav, true, 0);
        QCOMPARE(selected.main.value("id").toString(), QStringLiteral("navigation-session"));
        QVERIFY(selected.companion.isEmpty());
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

    void distantNavigationIsSelected()
    {
        QVariantMap nav{{"id", "navigation-session"}, {"kind", "nav"}, {"valid", true},
                        {"status", 2}, {"distance", 800.0}};
        const auto selected = AttentionPolicy::select({}, {}, nav, true, 0);
        QCOMPARE(selected.main.value("id").toString(), QStringLiteral("navigation-session"));
    }

    void navigationTextSurvivesSelectionAndUpdates()
    {
        NotificationService service(true);
        QVariantMap nav{{"id", "navigation-session"}, {"valid", true}, {"status", 2},
                        {"distance", 80.0}, {"instruction", "Turn right onto Main Street"},
                        {"compactInstruction", "Turn right"}};
        service.setNavigationPayload(nav);
        QCOMPARE(service.presentation().value("main").toMap().value("instruction").toString(),
                 QStringLiteral("Turn right onto Main Street"));
        service.publishCondition("critical", "engine", "Stop safely", {}, 0, "critical");
        QCOMPARE(service.presentation().value("companion").toMap().value("compactInstruction").toString(),
                 QStringLiteral("Turn right"));
        nav["compactInstruction"] = "Take the second exit";
        service.setNavigationPayload(nav);
        QCOMPARE(service.presentation().value("companion").toMap().value("compactInstruction").toString(),
                 QStringLiteral("Take the second exit"));
    }

    void presentationContainsNoLayout()
    {
        NotificationService service(true);
        service.publishCondition("fault", "engine", "Stop safely", "Details", 0, "critical");
        const auto presentation = service.presentation();
        QCOMPARE(presentation.size(), 3);
        QVERIFY(presentation.contains("main"));
        QVERIFY(presentation.contains("companion"));
        QVERIFY(presentation.contains("criticalCount"));
    }

    void calculatingCannotPresentStaleCompanion()
    {
        QVariantMap warning{{"id", "warning"}, {"priority", 2}};
        for (int status : {1, 3, 4, 5}) {
            QVariantMap nav{{"id", "navigation-session"}, {"valid", true}, {"status", status},
                            {"distance", 40.0}};
            QVERIFY(AttentionPolicy::select({warning}, {}, nav, true, 0).companion.isEmpty());
        }
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

    void telemetryLossDoesNotWaitForBanner()
    {
        NotificationService service;
        QSignalSpy trust(&service, &NotificationService::telemetryTrustChanged);
        QVERIFY(!service.telemetryTrustLost());
        service.setTelemetryConnected(false);
        QVERIFY(service.telemetryTrustLost());
        QCOMPARE(trust.count(), 1);
        QVERIFY(service.active().isEmpty());
        QVERIFY(service.presentation().value("main").toMap().isEmpty());
        service.setTelemetryConnected(false);
        QCOMPARE(trust.count(), 1);
        service.setTelemetryConnected(true);
        QVERIFY(!service.telemetryTrustLost());
        QCOMPARE(trust.count(), 2);
    }

    void prolongedDisconnectKeepsTrustLostUntilResolved()
    {
        NotificationService service;
        QSignalSpy trust(&service, &NotificationService::telemetryTrustChanged);
        service.setTelemetryConnected(false);
        service.publishCondition("redis-disconnect", "connection", "Disconnected", {}, 0);
        QCOMPARE(trust.count(), 1);
        service.setTelemetryConnected(true);
        QVERIFY(service.telemetryTrustLost());
        service.resolveCondition("redis-disconnect");
        QVERIFY(!service.telemetryTrustLost());
        QCOMPARE(trust.count(), 2);
    }

    void conditionResumeIsSilentUntilRecurrence()
    {
        NotificationService service;
        QSignalSpy cues(&service, &NotificationService::conditionPresented);
        service.publishCondition("warning", "test", "Warning", {}, 2);
        service.publishCondition("critical", "test", "Critical", {}, 0, "critical");
        QCOMPARE(cues.count(), 2);
        service.resolveCondition("critical");
        QCOMPARE(cues.count(), 2);
        service.publishCondition("warning", "test", "Escalated", {}, 0, "critical");
        QCOMPARE(cues.count(), 3);
        service.publishCondition("warning", "test", "Warning", {}, 2);
        service.publishCondition("warning", "test", "Escalated", {}, 0, "critical");
        QCOMPARE(cues.count(), 3);
        service.resolveCondition("warning");
        service.publishCondition("warning", "test", "Recurred", {}, 2);
        QCOMPARE(cues.count(), 4);
    }

    void deferredEventSoundsOnceWhenPresented()
    {
        NotificationService service;
        QSignalSpy cues(&service, &NotificationService::eventPresented);
        service.publishCondition("critical", "test", "Critical", {}, 0, "critical");
        service.publishEvent("event", "test", "Deferred", {}, 2, "warning");
        QCOMPARE(cues.count(), 0);
        service.resolveCondition("critical");
        QCOMPARE(cues.count(), 1);
        QCOMPARE(cues.at(0).at(0).toString(), QStringLiteral("warning"));
        service.publishCondition("critical", "test", "Critical", {}, 0, "critical");
        service.resolveCondition("critical");
        QCOMPARE(cues.count(), 1);
        service.clearEvent("event");
        service.publishEvent("event", "test", "Recurred", {}, 2, "warning");
        QCOMPARE(cues.count(), 2);
    }

    void eventUpdateReplacesClassificationAndEscalates()
    {
        NotificationService service;
        QSignalSpy cues(&service, &NotificationService::eventPresented);
        service.publishEvent("event", "test", "Initial", {}, 4, "info");
        service.publishCondition("warning", "test", "Warning", {}, 2);
        service.publishEvent("event", "updated-source", "Escalated", "Details", 0, "critical");
        const auto main = service.presentation().value("main").toMap();
        QCOMPARE(main.value("id").toString(), QStringLiteral("event"));
        QCOMPARE(main.value("source").toString(), QStringLiteral("updated-source"));
        QCOMPARE(main.value("priority").toInt(), 0);
        QCOMPARE(main.value("kind").toString(), QStringLiteral("critical"));
        QCOMPARE(main.value("revision").toInt(), 2);
        QCOMPARE(cues.count(), 2);
        QCOMPARE(cues.at(1).at(0).toString(), QStringLiteral("critical"));
        service.publishEvent("event", "updated-source", "Text update", {}, 0, "critical");
        QCOMPARE(cues.count(), 2);
    }

    void hiddenExpiredEventNeverSounds()
    {
        NotificationService service;
        QSignalSpy cues(&service, &NotificationService::eventPresented);
        service.publishCondition("critical", "test", "Critical", {}, 0, "critical");
        service.publishEvent("event", "test", "Stale", {}, 2, "warning", 10);
        QTest::qWait(300);
        service.resolveCondition("critical");
        QCOMPARE(cues.count(), 0);
        service.publishEvent("event", "test", "Fresh", {}, 2, "warning");
        QCOMPARE(cues.count(), 1);
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

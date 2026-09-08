#include <QtTest>
#include "services/AttentionPolicy.h"
#include "services/NotificationService.h"

class NotificationServiceTest : public QObject
{
    Q_OBJECT
    static QString mainId(const NotificationService &service)
    {
        return service.presentation().value("main").toMap().value("id").toString();
    }

    static void tick(NotificationService &service)
    {
        QVERIFY(QMetaObject::invokeMethod(&service, "expireEvents", Qt::DirectConnection));
    }

private slots:
    void equalSeverityCyclesAndWraps()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        QSignalSpy cues(&service, &NotificationService::conditionPresented);
        QSignalSpy changes(&service, &NotificationService::presentationChanged);
        service.publishCondition("a", "test", "A", {}, 0, "critical");
        service.publishCondition("b", "test", "B", {}, 0, "critical");
        service.publishCondition("c", "test", "C", {}, 0, "critical");
        service.publishCondition("lower", "test", "Lower", {}, 2);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        QCOMPARE(service.presentation().value("criticalCount").toInt(), 3);
        const int beforeTick = changes.count();
        now = 4999;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        QCOMPARE(changes.count(), beforeTick);
        for (const auto &id : {"b", "c", "a", "b"}) {
            now = ((now / 5000) + 1) * 5000;
            tick(service);
            QCOMPARE(mainId(service), QString::fromLatin1(id));
            QCOMPARE(service.presentation().value("criticalCount").toInt(), 3);
        }
        QCOMPARE(cues.count(), 3);
    }

    void singleItemAndNewArrivalKeepDwell()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        service.publishCondition("a", "test", "A", {}, 2);
        now = 2000;
        service.publishCondition("b", "test", "B", {}, 2);
        now = 4999;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        now = 5000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("b"));
        service.resolveCondition("a");
        QSignalSpy changes(&service, &NotificationService::presentationChanged);
        now = 25000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("b"));
        QCOMPARE(changes.count(), 0);
        service.publishCondition("c", "test", "C", {}, 2);
        QCOMPARE(mainId(service), QStringLiteral("c"));
        now = 29999;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("c"));
    }

    void producerAndNavigationChurnDoesNotResetDwell()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        service.publishCondition("a", "test", "A", {}, 2);
        service.publishEvent("b", "test", "B", {}, 2, "warning", 20000);
        QVariantMap nav{{"id", "navigation-session"}, {"valid", true}, {"status", 2}, {"distance", 800.0}};
        for (now = 100; now <= 10000; now += 100) {
            service.publishCondition("a", "test", QString::number(now), {}, 2);
            service.publishEvent("b", "test", QString::number(now), {}, 2, "warning", 20000);
            nav["distance"] = 800.0 + now;
            service.setNavigationPayload(nav);
            QCOMPARE(mainId(service), now < 5000 || now == 10000 ? QStringLiteral("a") : QStringLiteral("b"));
            QCOMPARE(service.presentation().value("companion").toMap().value("distance"), nav.value("distance"));
        }
    }

    void higherPriorityPreemptsAndEscalationRestartsDwell()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        QSignalSpy cues(&service, &NotificationService::conditionPresented);
        service.publishCondition("a", "test", "A", {}, 2);
        service.publishCondition("b", "test", "B", {}, 2);
        now = 5000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("b"));
        now = 6000;
        service.publishCondition("a", "test", "Critical A", {}, 0, "critical");
        QCOMPARE(mainId(service), QStringLiteral("a"));
        service.publishCondition("c", "test", "Critical C", {}, 0, "critical");
        now = 10999;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        now = 11000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("c"));
        service.resolveCondition("c");
        QCOMPARE(mainId(service), QStringLiteral("a"));
        QCOMPARE(cues.count(), 4);
        service.publishCondition("a", "test", "A", {}, 2);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        now = 15999;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        now = 16000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("b"));
        QCOMPARE(cues.count(), 4);
    }

    void removalAdvancesToSuccessorAndSkipsRemovedPeers()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        for (const auto &id : {"a", "b", "c", "d"})
            service.publishCondition(id, "test", id, {}, 2);
        now = 5000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("b"));
        service.resolveCondition("d");
        now = 6000;
        service.resolveCondition("b");
        QCOMPARE(mainId(service), QStringLiteral("c"));
        now = 10999;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("c"));
        now = 11000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        service.resolveCondition("c");
        service.resolveCondition("a");
        QVERIFY(mainId(service).isEmpty());
        now = 12000;
        service.publishCondition("a", "test", "Recurred", {}, 2);
        service.publishCondition("b", "test", "Recurred", {}, 2);
        now = 16999;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
    }

    void eventsRotateWithConditionsAndCuesStayDeduplicated()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        QSignalSpy conditions(&service, &NotificationService::conditionPresented);
        QSignalSpy events(&service, &NotificationService::eventPresented);
        service.publishEvent("event", "test", "Event", {}, 2, "warning", 30000);
        service.publishCondition("condition", "test", "Condition", {}, 2);
        service.publishEvent("other", "test", "Other", {}, 2, "error", 30000);
        for (const auto &id : {"condition", "other", "event", "condition", "other"}) {
            now += 5000;
            tick(service);
            QCOMPARE(mainId(service), QString::fromLatin1(id));
        }
        QCOMPARE(conditions.count(), 1);
        QCOMPARE(events.count(), 2);
        service.clearEvent("other");
        QCOMPARE(mainId(service), QStringLiteral("event"));
        now = 30000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("condition"));
        QCOMPARE(service.history().size(), 1);
        QCOMPARE(events.count(), 2);
    }

    void eventFreshnessIsNotExtendedByRotation()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        QSignalSpy cues(&service, &NotificationService::eventPresented);
        service.publishCondition("a", "test", "A", {}, 2);
        service.publishEvent("short", "test", "Short", {}, 2, "warning");
        service.publishEvent("long", "test", "Long", {}, 2, "warning", 6000);
        now = 5000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("long"));
        QCOMPARE(cues.count(), 1);
        QCOMPARE(service.history().size(), 1);
        now = 6000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        QCOMPARE(service.history().size(), 2);
        QCOMPARE(cues.count(), 1);
        now = 10000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
    }

    void surfaceEligibilityRemovesCurrentPeer()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        service.setSurface("map");
        service.publishCondition("a", "test", "A", {}, 2);
        service.setCoverageWarning(true);
        service.publishCondition("c", "test", "C", {}, 2);
        now = 5000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("map-coverage"));
        service.setSurface("cluster");
        QCOMPARE(mainId(service), QStringLiteral("c"));
        now = 10000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        now = 15000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("c"));
    }

    void navigationPreemptsRotationAndRemainsCompanion()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        service.publishCondition("a", "test", "A", {}, 2);
        service.publishCondition("b", "test", "B", {}, 2);
        QVariantMap nav{{"id", "navigation-session"}, {"valid", true}, {"status", 2}, {"distance", 800.0}};
        service.setNavigationPayload(nav);
        now = 5000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("b"));
        QCOMPARE(service.presentation().value("companion").toMap().value("id").toString(), QStringLiteral("navigation-session"));
        nav["distance"] = 40.0;
        service.setNavigationPayload(nav);
        QCOMPARE(mainId(service), QStringLiteral("navigation-session"));
        QVERIFY(service.presentation().value("companion").toMap().isEmpty());
        now = 20000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("navigation-session"));
        nav["distance"] = 800.0;
        service.setNavigationPayload(nav);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        now = 25000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("b"));
        service.publishCondition("a", "test", "A", {}, 3, "info");
        service.publishCondition("b", "test", "B", {}, 3, "info");
        QCOMPARE(mainId(service), QStringLiteral("navigation-session"));
        now = 30000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("navigation-session"));
    }

    void ridingEligibilityRemovesEventsFromCycle()
    {
        AttentionCycleState cycle;
        const QVariantMap a{{"id", "a"}, {"order", 1}, {"priority", 4}};
        const QVariantMap b{{"id", "b"}, {"order", 2}, {"priority", 4}, {"validUntil", 30000}};
        QCOMPARE(AttentionPolicy::select({a}, {b}, {}, false, 0, &cycle).main.value("id").toString(), QStringLiteral("a"));
        QCOMPARE(AttentionPolicy::select({a}, {b}, {}, false, 5000, &cycle).main.value("id").toString(), QStringLiteral("b"));
        QCOMPARE(AttentionPolicy::select({a}, {b}, {}, true, 6000, &cycle).main.value("id").toString(), QStringLiteral("a"));
        QCOMPARE(AttentionPolicy::select({a}, {b}, {}, true, 11000, &cycle).main.value("id").toString(), QStringLiteral("a"));
    }

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

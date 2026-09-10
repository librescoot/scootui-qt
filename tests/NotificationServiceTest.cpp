#include <QtTest>
#include "services/AttentionPolicy.h"
#include "services/NotificationService.h"
#include "services/NotificationIngress.h"
#include "services/ToastService.h"
#include "repositories/InMemoryMdbRepository.h"

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
    void toastAdapterMappingsAndLifetimes_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<int>("priority");
        QTest::addColumn<int>("ttl");
        QTest::newRow("error") << QStringLiteral("error") << 0 << 5000;
        QTest::newRow("warning") << QStringLiteral("warning") << 2 << 3000;
        QTest::newRow("success") << QStringLiteral("success") << 3 << 3000;
        QTest::newRow("info") << QStringLiteral("info") << 4 << 3000;
    }

    void toastAdapterMappingsAndLifetimes()
    {
        QFETCH(QString, kind);
        QFETCH(int, priority);
        QFETCH(int, ttl);
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        ToastService toast;
        toast.setNotificationService(&service);
        QSignalSpy cues(&service, &NotificationService::eventPresented);
        QSignalSpy legacyCues(&toast, &ToastService::toastAdded);
        const auto publish = [&] {
            if (kind == "error") toast.showError("Message");
            else if (kind == "warning") toast.showWarning("Message");
            else if (kind == "success") toast.showSuccess("Message");
            else toast.showInfo("Message");
        };
        publish();
        const auto entry = service.presentation().value("main").toMap();
        QCOMPARE(entry.value("kind").toString(), kind);
        QCOMPARE(entry.value("priority").toInt(), priority);
        QCOMPARE(entry.value("validUntil").toLongLong(), qint64(ttl));
        QCOMPARE(cues.count(), 1);
        QCOMPARE(cues.first().first().toString(), kind);
        publish();
        service.setSurface("map");
        QCOMPARE(cues.count(), 1);
        QCOMPARE(legacyCues.count(), 0);
        QVERIFY(toast.toasts().isEmpty());
        now = ttl - 1;
        tick(service);
        QVERIFY(!mainId(service).isEmpty());
        now = ttl;
        tick(service);
        QVERIFY(mainId(service).isEmpty());

        if (kind != "success") {
            QString id;
            if (kind == "error") id = toast.showPermanentError("Permanent", "permanent", "custom.svg");
            else if (kind == "warning") id = toast.showPermanentWarning("Permanent", "permanent", "custom.svg");
            else id = toast.showPermanentInfo("Permanent", "permanent", "custom.svg");
            const auto permanent = service.presentation().value("main").toMap();
            QCOMPARE(permanent.value("kind").toString(), kind);
            QCOMPARE(permanent.value("priority").toInt(), priority);
            QCOMPARE(permanent.value("icon").toString(), QStringLiteral("custom.svg"));
            now += 10000;
            tick(service);
            QCOMPARE(mainId(service), id);
            toast.dismiss(id);
            QVERIFY(mainId(service).isEmpty());
        }
    }

    void toastSuccessPreemptsInfoAndQueuesBySemanticKind()
    {
        NotificationService service;
        ToastService toast;
        toast.setNotificationService(&service);
        QSignalSpy cues(&service, &NotificationService::eventPresented);
        QSignalSpy legacyCues(&toast, &ToastService::toastAdded);
        toast.showInfo("Info");
        toast.showSuccess("Success");
        QCOMPARE(service.presentation().value("main").toMap().value("kind").toString(), QStringLiteral("success"));
        QCOMPARE(service.presentation().value("queuedCounts").toMap(), (QVariantMap{{"info", 1}}));
        toast.showWarning("Warning");
        QCOMPARE(service.presentation().value("queuedCounts").toMap(), (QVariantMap{{"success", 1}, {"info", 1}}));
        service.clearEvent(mainId(service));
        QCOMPARE(service.presentation().value("main").toMap().value("kind").toString(), QStringLiteral("success"));
        toast.showSuccess("Success");
        QCOMPARE(cues.count(), 3);
        QCOMPARE(cues.at(1).first().toString(), QStringLiteral("success"));
        QCOMPARE(legacyCues.count(), 0);
    }

    void nativeEventDefaultLifetimeUnchanged()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        service.publishEvent("native", "test", "Native", {});
        now = 3999;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("native"));
        now = 4000;
        tick(service);
        QVERIFY(mainId(service).isEmpty());
    }

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
        service.publishEvent("other", "test", "Other", {}, 2, "warning", 30000);
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

    void ingressDefaultLifetimeStartsAtPublication()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        InMemoryMdbRepository repository;
        NotificationIngress ingress(&repository, &service);
        service.publishCondition("warning", "test", "Warning", {}, 2);
        QVERIFY(ingress.receive(QStringLiteral(R"({"id":"default","title":"Default lifetime"})")));
        now = 9999;
        tick(service);
        service.resolveCondition("warning");
        QCOMPARE(mainId(service), QStringLiteral("external:external:default"));
        now = 10000;
        tick(service);
        QVERIFY(mainId(service).isEmpty());
        QCOMPARE(service.history().size(), 1);
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

    void navigationNeverPreemptsWarningsAndLowerSeveritiesUseCompanion()
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
        nav["distance"] = 40.0;
        service.setNavigationPayload(nav);
        QCOMPARE(mainId(service), QStringLiteral("b"));
        QCOMPARE(service.presentation().value("companion").toMap().value("id").toString(), QStringLiteral("navigation-session"));
        now = 10000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("a"));
        service.publishCondition("a", "test", "A", {}, 4, "info");
        service.publishCondition("b", "test", "B", {}, 4, "info");
        QCOMPARE(mainId(service), QStringLiteral("navigation-session"));
        QCOMPARE(service.presentation().value("companion").toMap().value("id").toString(), QStringLiteral("a"));
        now = 15000;
        tick(service);
        QCOMPARE(service.presentation().value("companion").toMap().value("id").toString(), QStringLiteral("b"));
    }

    void secondaryDwellCountsExpiryAndCues()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        QSignalSpy cues(&service, &NotificationService::eventPresented);
        QVariantMap nav{{"id", "navigation-session"}, {"valid", true}, {"status", 2}, {"distance", 80.0}};
        service.setNavigationPayload(nav);
        service.publishEvent("a", "test", "A", {}, 4, "info", 10000);
        service.publishEvent("b", "test", "B", {}, 4, "info", 10000);
        service.publishEvent("c", "test", "C", {}, 4, "info", 10000);
        QCOMPARE(service.presentation().value("queuedCounts").toMap().value("info").toInt(), 2);
        for (now = 100; now < 5000; now += 100) {
            nav["distance"] = 80.0 + now;
            service.setNavigationPayload(nav);
            QCOMPARE(service.presentation().value("companion").toMap().value("id").toString(), QStringLiteral("a"));
        }
        tick(service);
        QCOMPARE(service.presentation().value("companion").toMap().value("id").toString(), QStringLiteral("b"));
        QCOMPARE(cues.count(), 2);
        service.clearEvent("b");
        QCOMPARE(service.presentation().value("companion").toMap().value("id").toString(), QStringLiteral("c"));
        QCOMPARE(service.presentation().value("queuedCounts").toMap().value("info").toInt(), 1);
        now = 10000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("navigation-session"));
        QVERIFY(service.presentation().value("companion").toMap().isEmpty());
        QVERIFY(service.presentation().value("queuedCounts").toMap().isEmpty());
        QCOMPARE(cues.count(), 3);
        QCOMPARE(service.history().size(), 2);
    }

    void secondaryCyclesWrapWithoutRepeatingCues()
    {
        qint64 now = 0;
        NotificationService service(false, nullptr, [&now] { return now; });
        QSignalSpy cues(&service, &NotificationService::eventPresented);
        QVariantMap nav{{"id", "navigation-session"}, {"valid", true}, {"status", 2}};
        service.setNavigationPayload(nav);
        service.publishEvent("a", "test", "A", {}, 4, "info", 30000);
        service.publishEvent("b", "test", "B", {}, 4, "info", 30000);
        for (now = 100; now <= 20000; now += 100) {
            service.publishEvent("a", "test", QString::number(now), {}, 4, "info", 30000);
            nav["distance"] = now;
            service.setNavigationPayload(nav);
            const QString expected = (now / 5000) % 2 == 0 ? QStringLiteral("a") : QStringLiteral("b");
            QCOMPARE(service.presentation().value("companion").toMap().value("id").toString(), expected);
            QCOMPARE(mainId(service), QStringLiteral("navigation-session"));
        }
        QCOMPARE(cues.count(), 2);
    }

    void severityOrderAndQueuedCounts()
    {
        NotificationService service;
        InMemoryMdbRepository repository;
        NotificationIngress ingress(&repository, &service);
        service.setNavigationPayload({{"id", "navigation-session"}, {"valid", true}, {"status", 2}});
        for (const auto &kind : {"debug", "info", "success", "warning", "error", "critical"})
            QVERIFY(ingress.receive(QStringLiteral(R"({"id":"%1","title":"%1","severity":"%1"})").arg(kind)));
        QCOMPARE(mainId(service), QStringLiteral("external:external:error"));
        auto counts = service.presentation().value("queuedCounts").toMap();
        QCOMPARE(counts, (QVariantMap{{"error", 1}, {"warning", 1}, {"success", 1}, {"info", 1}, {"debug", 1}}));
        for (const auto &kind : {"error", "critical", "warning"})
            service.clearEvent(QStringLiteral("external:external:%1").arg(kind));
        QCOMPARE(mainId(service), QStringLiteral("navigation-session"));
        QCOMPARE(service.presentation().value("companion").toMap().value("kind").toString(), QStringLiteral("success"));
        QCOMPARE(service.presentation().value("queuedCounts").toMap(), (QVariantMap{{"info", 1}, {"debug", 1}}));
        service.clearEvent("external:external:success");
        QCOMPARE(service.presentation().value("companion").toMap().value("kind").toString(), QStringLiteral("info"));
        service.clearEvent("external:external:info");
        QCOMPARE(service.presentation().value("companion").toMap().value("kind").toString(), QStringLiteral("debug"));
    }

    void ridingDoesNotSuppressInformationalEvents()
    {
        AttentionCycleState cycle;
        const QVariantMap a{{"id", "a"}, {"order", 1}, {"priority", 4}};
        const QVariantMap b{{"id", "b"}, {"order", 2}, {"priority", 4}, {"validUntil", 30000}};
        QCOMPARE(AttentionPolicy::select({a}, {b}, {}, false, 0, &cycle).main.value("id").toString(), QStringLiteral("a"));
        QCOMPARE(AttentionPolicy::select({a}, {b}, {}, false, 5000, &cycle).main.value("id").toString(), QStringLiteral("b"));
        QCOMPARE(AttentionPolicy::select({a}, {b}, {}, true, 6000, &cycle).main.value("id").toString(), QStringLiteral("b"));
        QCOMPARE(AttentionPolicy::select({a}, {b}, {}, true, 11000, &cycle).main.value("id").toString(), QStringLiteral("a"));
        QCOMPARE(AttentionPolicy::select({}, {b}, {}, true, 12000).main.value("id").toString(), QStringLiteral("b"));
        QVERIFY(AttentionPolicy::select({}, {b}, {}, true, 30000).main.isEmpty());
    }

    void externalNotificationPubsubUpdateAndDismiss()
    {
        InMemoryMdbRepository repository;
        NotificationService service;
        NotificationIngress ingress(&repository, &service);
        repository.publish("scootui:notification",
                           R"({"source":"script","id":"job","title":"Started","severity":"info"})");
        QCOMPARE(mainId(service), QStringLiteral("external:script:job"));
        QCOMPARE(service.presentation().value("main").toMap().value("priority").toInt(), 4);
        repository.publish("scootui:notification",
                           R"({"source":"script","id":"job","title":"Done","severity":"success"})");
        QCOMPARE(service.presentation().value("main").toMap().value("title").toString(),
                 QStringLiteral("Done"));
        repository.publish("scootui:notification",
                           R"({"source":"script","id":"job","action":"dismiss"})");
        QVERIFY(service.presentation().value("main").toMap().isEmpty());
    }

    void externalIdsCannotDismissBuiltInsOrOtherSources()
    {
        InMemoryMdbRepository repository;
        NotificationService service;
        NotificationIngress ingress(&repository, &service);
        service.publishEvent("ecu-fault", "ecu", "Built in", {}, 0, "critical", 60000);
        QVERIFY(ingress.receive(R"({"id":"ecu-fault","action":"dismiss"})"));
        QCOMPARE(mainId(service), QStringLiteral("ecu-fault"));
        service.clearEvent("ecu-fault");
        QVERIFY(ingress.receive(R"({"source":"one","id":"job","title":"First"})"));
        QVERIFY(ingress.receive(R"({"source":"two","id":"job","action":"dismiss"})"));
        QCOMPARE(mainId(service), QStringLiteral("external:one:job"));
    }

    void externalMalformedMessagesHaveNoEffects()
    {
        InMemoryMdbRepository repository;
        NotificationService service;
        NotificationIngress ingress(&repository, &service);
        QSignalSpy rejected(&ingress, &NotificationIngress::rejected);
        repository.publish("scootui:notification", "*");
        QCOMPARE(rejected.count(), 0);

        const QStringList invalid = {
            "not json",
            "[]",
            "null",
            "{}",
            R"({"id":"a","title":" "})",
            R"({"id":"a","title":true})",
            R"({"id":"a","title":"Hi","body":7})",
            R"({"id":"a","title":"Hi","severity":"urgent"})",
            R"({"id":"a","title":"Hi","priority":0})",
            R"({"id":"a","title":"Hi","ttl_ms":0})",
            R"({"id":"a","title":"Hi","ttl_ms":60001})",
            R"({"id":"a","title":"Hi","ttl_ms":1000.5})",
            R"({"id":"a","title":"Hi","ttl_ms":"1000"})",
            R"({"id":"a","title":"Hi","ttl_ms":null})",
            R"({"id":"a","title":"Hi","source":"invalid:source"})",
            R"({"id":"a:b","title":"Hi"})",
            R"({"id":"a\n","title":"Hi"})",
            R"({"id":"a","source":"test\n","title":"Hi"})",
            R"({"id":"a","title":"Hi","action":"resolve"})",
            R"({"id":"a","action":"dismiss","ttl_ms":5000})",
            QString(4097, 'x'),
            QStringLiteral("{\"id\":\"a\",\"title\":\"%1\"}").arg(QString(121, 'x')),
            QStringLiteral("{\"id\":\"a\",\"title\":\"Hi\",\"body\":\"%1\"}")
                .arg(QString(513, 'x'))};
        for (const auto &message : invalid) {
            QVERIFY2(!ingress.receive(message), qPrintable(message));
            QVERIFY(service.presentation().value("main").toMap().isEmpty());
            QVERIFY(service.history().isEmpty());
        }
        QCOMPARE(rejected.count(), invalid.size());
    }

    void externalExpiryAndCriticalCycling()
    {
        qint64 now = 0;
        InMemoryMdbRepository repository;
        NotificationService service(false, nullptr, [&now] { return now; });
        NotificationIngress ingress(&repository, &service);
        QVERIFY(
            ingress.receive(R"({"id":"a","title":"First","severity":"critical","ttl_ms":10000})"));
        QVERIFY(
            ingress.receive(R"({"id":"b","title":"Second","severity":"critical","ttl_ms":10000})"));
        QCOMPARE(service.presentation().value("criticalCount").toInt(), 2);
        QCOMPARE(mainId(service), QStringLiteral("external:external:a"));
        now = 5000;
        tick(service);
        QCOMPARE(mainId(service), QStringLiteral("external:external:b"));
        now = 10000;
        tick(service);
        QVERIFY(service.presentation().value("main").toMap().isEmpty());
        QCOMPARE(service.presentation().value("criticalCount").toInt(), 0);
        QCOMPARE(service.history().size(), 2);
    }

    void externalIngressUnsubscribesIndependently()
    {
        InMemoryMdbRepository repository;
        NotificationService service;
        int otherCalls = 0;
        repository.subscribe("scootui:notification",
                             [&otherCalls](const QString &, const QString &) { ++otherCalls; });
        auto *ingress = new NotificationIngress(&repository, &service);
        delete ingress;
        repository.publish("scootui:notification", R"({"id":"a","title":"Hi"})");
        QCOMPARE(otherCalls, 1);
        QVERIFY(service.presentation().value("main").toMap().isEmpty());
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

    void imminentNavigationCannotHideWarning()
    {
        QVariantMap warning{{"id", "warning"}, {"priority", 2}, {"kind", "warning"}};
        QVariantMap nav{{"id", "navigation-session"}, {"kind", "nav"}, {"valid", true},
                        {"status", 2}, {"distance", 40.0}};
        const auto selected = AttentionPolicy::select({warning}, {}, nav, true, 0);
        QCOMPARE(selected.main.value("id").toString(), QStringLiteral("warning"));
        QCOMPARE(selected.companion.value("id").toString(), QStringLiteral("navigation-session"));
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
        QCOMPARE(presentation.size(), 4);
        QVERIFY(presentation.contains("queuedCounts"));
        QVERIFY(presentation.contains("main"));
        QVERIFY(presentation.contains("companion"));
        QVERIFY(presentation.contains("criticalCount"));
    }

    void calculatingCannotPresentStaleCompanion()
    {
        QVariantMap warning{{"id", "warning"}, {"priority", 2}};
        for (int status : {1, 3, 5}) {
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

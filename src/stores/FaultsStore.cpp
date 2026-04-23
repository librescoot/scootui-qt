#include "FaultsStore.h"

#include "BatteryStore.h"
#include "EngineStore.h"
#include "VehicleStore.h"
#include "BluetoothStore.h"
#include "InternetStore.h"
#include "FaultEventStore.h"
#include "l10n/Translations.h"
#include "utils/FaultFormatter.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <algorithm>

namespace {

// Aggregated data per (source, |code|) across the stream window.
struct Accum {
    int raiseCount = 0;
    qint64 firstRaisedMs = 0;
    qint64 lastRaisedMs = 0;
    qint64 clearedAtMs = 0;
    QString description;  // from latest raise
};

QString sourceForBatterySlot(const BatteryStore *bs)
{
    if (!bs) return {};
    return QStringLiteral("battery:") + bs->batteryId();
}

void mergeActive(QSet<QPair<QString, int>> &active, const QString &source,
                 const QList<int> &codes)
{
    for (int c : codes)
        active.insert({source, c});
}

}

FaultsStore::FaultsStore(BatteryStore *battery0, BatteryStore *battery1,
                         EngineStore *engine, VehicleStore *vehicle,
                         BluetoothStore *bluetooth, InternetStore *internet,
                         FaultEventStore *events, Translations *translations,
                         QObject *parent)
    : QObject(parent)
    , m_battery0(battery0)
    , m_battery1(battery1)
    , m_engine(engine)
    , m_vehicle(vehicle)
    , m_bluetooth(bluetooth)
    , m_internet(internet)
    , m_events(events)
    , m_translations(translations)
{
    if (m_battery0)
        connect(m_battery0, &BatteryStore::faultsChanged, this, &FaultsStore::rebuild);
    if (m_battery1)
        connect(m_battery1, &BatteryStore::faultsChanged, this, &FaultsStore::rebuild);
    if (m_engine)
        connect(m_engine, &EngineStore::faultsChanged, this, &FaultsStore::rebuild);
    if (m_vehicle)
        connect(m_vehicle, &VehicleStore::faultsChanged, this, &FaultsStore::rebuild);
    if (m_bluetooth)
        connect(m_bluetooth, &BluetoothStore::faultsChanged, this, &FaultsStore::rebuild);
    if (m_internet)
        connect(m_internet, &InternetStore::faultsChanged, this, &FaultsStore::rebuild);
    if (m_events)
        connect(m_events, &FaultEventStore::eventsChanged, this, &FaultsStore::rebuild);

    rebuild();
}

void FaultsStore::rebuild()
{
    // 1. Collect current active set from every fault-bearing store.
    QSet<QPair<QString, int>> active;
    if (m_battery0)
        mergeActive(active, sourceForBatterySlot(m_battery0), m_battery0->faults());
    if (m_battery1)
        mergeActive(active, sourceForBatterySlot(m_battery1), m_battery1->faults());
    if (m_engine)
        mergeActive(active, QStringLiteral("engine-ecu"), m_engine->faults());
    if (m_vehicle)
        mergeActive(active, QStringLiteral("vehicle"), m_vehicle->faults());
    if (m_bluetooth)
        mergeActive(active, QStringLiteral("ble"), m_bluetooth->faults());
    if (m_internet)
        mergeActive(active, QStringLiteral("internet"), m_internet->faults());

    // 2. Walk stream events (newest-first from XREVRANGE) and accumulate per
    // (source, |code|). Remember: positive code is a raise, negative a clear.
    QHash<QPair<QString, int>, Accum> agg;
    if (m_events) {
        for (const FaultEvent &e : m_events->events()) {
            const int absCode = e.code < 0 ? -e.code : e.code;
            if (absCode == 0 || e.source.isEmpty())
                continue;
            const QPair<QString, int> key{e.source, absCode};
            Accum &a = agg[key];
            if (e.code > 0) {
                a.raiseCount++;
                if (a.lastRaisedMs == 0 || e.timestampMs > a.lastRaisedMs) {
                    a.lastRaisedMs = e.timestampMs;
                    if (!e.description.isEmpty())
                        a.description = e.description;
                }
                if (a.firstRaisedMs == 0 || e.timestampMs < a.firstRaisedMs)
                    a.firstRaisedMs = e.timestampMs;
            } else {
                if (a.clearedAtMs == 0 || e.timestampMs > a.clearedAtMs)
                    a.clearedAtMs = e.timestampMs;
            }
        }
    }

    // 3. Union of keys from both sources. An active fault might have no stream
    // entry (stream window shorter than fault age); a cleared fault has no
    // presence in the active set.
    QSet<QPair<QString, int>> keys = active;
    for (auto it = agg.cbegin(); it != agg.cend(); ++it)
        keys.insert(it.key());

    // 4. Build entry list.
    QVariantList list;
    list.reserve(keys.size());
    int newActiveCount = 0;

    for (const auto &key : keys) {
        const QString &source = key.first;
        const int code = key.second;
        const bool isActive = active.contains(key);
        const Accum &a = agg.value(key);

        QString description = a.description;
        if (description.isEmpty() && m_translations)
            description = FaultFormatter::describeFault(source, code, m_translations);

        const FaultSeverity sev = FaultFormatter::faultSeverity(source, code);
        if (isActive)
            ++newActiveCount;

        const qint64 lastSeen = std::max({a.lastRaisedMs, a.clearedAtMs});

        QVariantMap entry;
        entry.insert(QStringLiteral("active"), isActive);
        entry.insert(QStringLiteral("source"), source);
        entry.insert(QStringLiteral("sourceLabel"), FaultFormatter::sourceLabel(source));
        entry.insert(QStringLiteral("code"), code);
        entry.insert(QStringLiteral("codeLabel"), FaultFormatter::codeLabel(source, code));
        entry.insert(QStringLiteral("description"), description);
        entry.insert(QStringLiteral("severity"), static_cast<int>(sev));
        entry.insert(QStringLiteral("firstRaisedMs"), a.firstRaisedMs);
        entry.insert(QStringLiteral("lastRaisedMs"), a.lastRaisedMs);
        entry.insert(QStringLiteral("clearedAtMs"), a.clearedAtMs);
        entry.insert(QStringLiteral("raiseCount"), a.raiseCount);
        entry.insert(QStringLiteral("lastSeenMs"), lastSeen);
        list.append(entry);
    }

    // 5. Sort: active first (desc by lastSeen), then cleared (desc by lastSeen).
    std::sort(list.begin(), list.end(), [](const QVariant &lv, const QVariant &rv) {
        const QVariantMap l = lv.toMap();
        const QVariantMap r = rv.toMap();
        const bool la = l.value(QStringLiteral("active")).toBool();
        const bool ra = r.value(QStringLiteral("active")).toBool();
        if (la != ra)
            return la;
        return l.value(QStringLiteral("lastSeenMs")).toLongLong()
             > r.value(QStringLiteral("lastSeenMs")).toLongLong();
    });

    m_entries = list;
    m_activeCount = newActiveCount;
    emit entriesChanged();
}

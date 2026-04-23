#pragma once

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVariantList>

class BatteryStore;
class EngineStore;
class VehicleStore;
class BluetoothStore;
class InternetStore;
class FaultEventStore;
class Translations;

// Aggregates active fault sets across services with history entries from the
// events:faults stream, deduped per (source, |code|). Active entries come
// first (ordered by lastSeen desc), cleared entries follow. The screen uses
// `entries`; the menu uses `activeCount` to show the "(N)" badge and decide
// whether the root-menu item is visible.
class FaultsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY entriesChanged)

public:
    explicit FaultsStore(BatteryStore *battery0,
                         BatteryStore *battery1,
                         EngineStore *engine,
                         VehicleStore *vehicle,
                         BluetoothStore *bluetooth,
                         InternetStore *internet,
                         FaultEventStore *events,
                         Translations *translations,
                         QObject *parent = nullptr);

    QVariantList entries() const { return m_entries; }
    int activeCount() const { return m_activeCount; }

signals:
    void entriesChanged();

private slots:
    void rebuild();

private:
    QPointer<BatteryStore> m_battery0;
    QPointer<BatteryStore> m_battery1;
    QPointer<EngineStore> m_engine;
    QPointer<VehicleStore> m_vehicle;
    QPointer<BluetoothStore> m_bluetooth;
    QPointer<InternetStore> m_internet;
    QPointer<FaultEventStore> m_events;
    QPointer<Translations> m_translations;

    QVariantList m_entries;
    int m_activeCount = 0;
};

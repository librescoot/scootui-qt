#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class EngineStore;
class VehicleStore;
class ConnectionStore;
class SettingsStore;
class SettingsService;
class DataPartition;

class OdometerMilestoneService : public QObject
{
    Q_OBJECT

public:
    OdometerMilestoneService(EngineStore *engineStore,
                             VehicleStore *vehicleStore,
                             ConnectionStore *connectionStore,
                             SettingsStore *settingsStore,
                             DataPartition *dataPartition = nullptr,
                             QObject *parent = nullptr);

    void setSettingsService(SettingsService *service);

    // Called by the big celebration overlay when its hold finishes, to
    // request the next queued milestone (if any).
    Q_INVOKABLE void advanceCelebration();

    // Easter-egg menu test actions. celebrateRandomEasterEgg() queues one of
    // the one-shot eggs so the confetti, banner and toast can be seen without
    // riding 666 km, and deliberately does not consume it: it is a demo, not a
    // crossing. resetEasterEggs() forgets which eggs have fired so they can be
    // earned again.
    Q_INVOKABLE void celebrateRandomEasterEgg();
    Q_INVOKABLE void resetEasterEggs();
    Q_INVOKABLE int firedEasterEggCount() const { return m_firedEasterEggs.size(); }

signals:
    void firedEasterEggsChanged();

    // Fired the instant a milestone is crossed during a ride. Drives the
    // small in-ride toast only. No queueing; one event per crossing.
    // km: display value in kilometers (integer for plain milestones,
    //     fractional for easter-egg numbers).
    // intensity: 1..10, used by the celebration to scale confetti.
    // tag: empty string for plain milestones, or a one-shot easter-egg id.
    void milestoneCrossed(double km, int intensity, QString tag);

    // Fired when the menu's random demo reaches the front of the queue.
    void milestoneDemoStarted(int intensity);

    // Fired one-at-a-time when the scooter parks with one or more queued
    // crossings from the ride. Drives confetti + the big centered banner.
    // The overlay calls advanceCelebration() when its hold finishes to
    // pop the next item, or end the sequence.
    void milestoneCelebrate(double km, int intensity, QString tag);

private:
    struct Pending {
        double km;
        int intensity;
        QString tag;
        bool demo = false;
    };

    static int milestoneForKm(double km);
    static int intensityForMilestone(int milestoneKm);

    void onOdometerChanged();
    void onVehicleStateChanged();
    void enqueueAndCross(double km, int intensity, const QString &tag, bool demo = false);
    void startNextCelebration();

    QString milestoneMode() const;
    QString milestonePresentation() const;
    void migrateLegacyEasterEggs();

    // Decides whether enough is known to start celebrating. Runs on a repeating
    // timer until a real odometer reading has arrived and the persisted state
    // is reachable; see the definition.
    void trySettle();

    // Before /data is mounted the files at the persist paths are rootfs shadow
    // copies, so reads wait for the mount and writes queue in m_pendingWrites.
    bool dataReady() const;
    void loadPersistedState();
    void onDataMounted();
    void persist(const QString &path, const QByteArray &contents);

    QString persistPath() const;
    int loadLastMilestone() const;
    void saveLastMilestone(int km);

    // Easter eggs are one-shot for the life of the vehicle, so which ones have
    // already fired has to outlive the process.
    QString firedEggsPath() const;
    QSet<QString> loadFiredEasterEggs() const;
    void saveFiredEasterEggs();
    // Marks tag fired and persists the set. Returns false if it had already
    // fired, so callers can skip the rest of the work.
    bool markEasterEggFired(const QString &tag);

    EngineStore *m_engineStore = nullptr;
    VehicleStore *m_vehicleStore = nullptr;
    ConnectionStore *m_connectionStore = nullptr;
    SettingsStore *m_settingsStore = nullptr;
    SettingsService *m_settingsService = nullptr;
    DataPartition *m_dataPartition = nullptr;
    QHash<QString, QByteArray> m_pendingWrites;

    int m_lastCelebrated = -1;
    bool m_settled = false;
    double m_maxSeenDuringSettle = 0.0;
    double m_lastOdoKm = -1.0;
    bool m_legacyMigrationInFlight = false;
    QSet<QString> m_firedEasterEggs;

    QList<Pending> m_queue;
    bool m_celebrating = false;
    int m_lastVehicleState = -1;
    QTimer *m_settleTimer = nullptr;

    QString easterEggsPath() const;
};

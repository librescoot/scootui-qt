#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlengine.h>

class EngineStore;
class VehicleStore;
class ConnectionStore;
class SettingsStore;

class OdometerMilestoneService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool easterEggsEnabled READ easterEggsEnabled WRITE setEasterEggsEnabled NOTIFY easterEggsEnabledChanged)

public:
    OdometerMilestoneService(EngineStore *engineStore,
                             VehicleStore *vehicleStore,
                             ConnectionStore *connectionStore,
                             SettingsStore *settingsStore,
                             QObject *parent = nullptr);

    bool easterEggsEnabled() const { return m_easterEggsEnabled; }
    void setEasterEggsEnabled(bool enabled);

    // Called by the big celebration overlay when its hold finishes, to
    // request the next queued milestone (if any).
    Q_INVOKABLE void advanceCelebration();

signals:
    void easterEggsEnabledChanged();

    // Fired the instant a milestone is crossed during a ride. Drives the
    // small in-ride toast only. No queueing; one event per crossing.
    // km: display value in kilometers (integer for plain milestones,
    //     fractional for easter-egg numbers).
    // intensity: 1..10, used by the celebration to scale confetti.
    // tag: empty string for plain milestones, or an id like "devil",
    //      "leet", "power2", "sequence", "boobs", "rollover".
    void milestoneCrossed(double km, int intensity, QString tag);

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
    };

    static int milestoneForKm(double km);
    static int intensityForMilestone(int milestoneKm);

    void onOdometerChanged();
    void onVehicleStateChanged();
    void enqueueAndCross(double km, int intensity, const QString &tag);
    void startNextCelebration();

    // Master on/off for all milestone output (confetti, banner, toast,
    // easter eggs). Null store => enabled, so tests/simulator without a
    // wired SettingsStore keep celebrating.
    bool celebrationsEnabled() const;

    // Decides whether enough is known to start celebrating. Runs on a repeating
    // timer until a real odometer reading has arrived; see the definition.
    void trySettle();

    QString persistPath() const;
    int loadLastMilestone() const;
    void saveLastMilestone(int km);

    // Easter eggs are one-shot for the life of the vehicle, so which ones have
    // already fired has to outlive the process.
    QString firedEggsPath() const;
    QSet<QString> loadFiredEasterEggs() const;
    void saveFiredEasterEggs() const;
    // Marks tag fired and persists the set. Returns false if it had already
    // fired, so callers can skip the rest of the work.
    bool markEasterEggFired(const QString &tag);

    EngineStore *m_engineStore = nullptr;
    VehicleStore *m_vehicleStore = nullptr;
    ConnectionStore *m_connectionStore = nullptr;
    SettingsStore *m_settingsStore = nullptr;

    int m_lastCelebrated = -1;
    bool m_settled = false;
    double m_maxSeenDuringSettle = 0.0;
    double m_lastOdoKm = -1.0;
    bool m_easterEggsEnabled = false;
    QSet<QString> m_firedEasterEggs;

    QList<Pending> m_queue;
    bool m_celebrating = false;
    int m_lastVehicleState = -1;
    QTimer *m_settleTimer = nullptr;

    QString easterEggsPath() const;
    bool loadEasterEggsEnabled() const;
    void saveEasterEggsEnabled(bool enabled);

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static OdometerMilestoneService *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(OdometerMilestoneService *instance) { s_qmlInstance = instance; }

private:
    static inline OdometerMilestoneService *s_qmlInstance = nullptr;
};

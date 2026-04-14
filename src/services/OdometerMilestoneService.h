#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class EngineStore;
class VehicleStore;
class ConnectionStore;

class OdometerMilestoneService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double currentKm READ currentKm NOTIFY milestoneReached)
    Q_PROPERTY(int currentIntensity READ currentIntensity NOTIFY milestoneReached)
    Q_PROPERTY(QString currentTag READ currentTag NOTIFY milestoneReached)
    Q_PROPERTY(bool easterEggsEnabled READ easterEggsEnabled WRITE setEasterEggsEnabled NOTIFY easterEggsEnabledChanged)

public:
    OdometerMilestoneService(EngineStore *engineStore,
                             VehicleStore *vehicleStore,
                             ConnectionStore *connectionStore,
                             QObject *parent = nullptr);

    double currentKm() const { return m_currentKm; }
    int currentIntensity() const { return m_currentIntensity; }
    QString currentTag() const { return m_currentTag; }
    bool easterEggsEnabled() const { return m_easterEggsEnabled; }
    void setEasterEggsEnabled(bool enabled);

    Q_INVOKABLE void dismiss();

signals:
    void easterEggsEnabledChanged();
    // km: display value in kilometers (integer for plain milestones,
    //     fractional for easter-egg numbers).
    // intensity: 1..10, controls confetti amount.
    // tag: empty string for plain milestones, or an id like "devil",
    //      "leet", "power2", "sequence", "boobs", "rollover".
    void milestoneReached(double km, int intensity, QString tag);

private:
    static int milestoneForKm(double km);
    static int intensityForMilestone(int milestoneKm);

    void onOdometerChanged();
    QString persistPath() const;
    int loadLastMilestone() const;
    void saveLastMilestone(int km);

    EngineStore *m_engineStore = nullptr;
    VehicleStore *m_vehicleStore = nullptr;
    ConnectionStore *m_connectionStore = nullptr;

    int m_lastCelebrated = -1;
    double m_currentKm = 0.0;
    int m_currentIntensity = 0;
    QString m_currentTag;
    bool m_settled = false;
    double m_maxSeenDuringSettle = 0.0;
    double m_lastOdoKm = -1.0;
    bool m_easterEggsEnabled = false;
    QSet<QString> m_firedEasterEggs;

    QString easterEggsPath() const;
    bool loadEasterEggsEnabled() const;
    void saveEasterEggsEnabled(bool enabled);
};

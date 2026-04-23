#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QSet>

class VehicleStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(int blinkerState READ blinkerState NOTIFY blinkerStateChanged)
    Q_PROPERTY(int blinkerSwitch READ blinkerSwitch NOTIFY blinkerSwitchChanged)
    Q_PROPERTY(qreal blinkOpacity READ blinkOpacity NOTIFY blinkOpacityChanged)
    Q_PROPERTY(int brakeLeft READ brakeLeft NOTIFY brakeLeftChanged)
    Q_PROPERTY(int brakeRight READ brakeRight NOTIFY brakeRightChanged)
    Q_PROPERTY(int kickstand READ kickstand NOTIFY kickstandChanged)
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString stateRaw READ stateRaw NOTIFY stateRawChanged)
    Q_PROPERTY(int handleBarLockSensor READ handleBarLockSensor NOTIFY handleBarLockSensorChanged)
    Q_PROPERTY(bool handlebarInLockPosition READ handlebarInLockPosition NOTIFY handlebarInLockPositionChanged)
    Q_PROPERTY(int seatboxButton READ seatboxButton NOTIFY seatboxButtonChanged)
    Q_PROPERTY(int seatboxLock READ seatboxLock NOTIFY seatboxLockChanged)
    Q_PROPERTY(int hornButton READ hornButton NOTIFY hornButtonChanged)
    Q_PROPERTY(int isUnableToDrive READ isUnableToDrive NOTIFY isUnableToDriveChanged)
    Q_PROPERTY(bool hopOnActive READ hopOnActive NOTIFY hopOnActiveChanged)
    Q_PROPERTY(int mainPower READ mainPower NOTIFY mainPowerChanged)
    Q_PROPERTY(QList<int> faults READ faults NOTIFY faultsChanged)

public:
    explicit VehicleStore(MdbRepository *repo, QObject *parent = nullptr);
    ~VehicleStore() override;

    int blinkerState() const { return static_cast<int>(m_blinkerState); }
    int blinkerSwitch() const { return static_cast<int>(m_blinkerSwitch); }
    qreal blinkOpacity() const { return m_blinkOpacity; }
    int brakeLeft() const { return static_cast<int>(m_brakeLeft); }
    int brakeRight() const { return static_cast<int>(m_brakeRight); }
    int kickstand() const { return static_cast<int>(m_kickstand); }
    int state() const { return static_cast<int>(m_state); }
    QString stateRaw() const { return m_stateRaw; }
    int handleBarLockSensor() const { return static_cast<int>(m_handleBarLockSensor); }
    bool handlebarInLockPosition() const { return m_handlebarInLockPosition; }
    int seatboxButton() const { return static_cast<int>(m_seatboxButton); }
    int seatboxLock() const { return static_cast<int>(m_seatboxLock); }
    int hornButton() const { return static_cast<int>(m_hornButton); }
    int isUnableToDrive() const { return static_cast<int>(m_isUnableToDrive); }
    bool hopOnActive() const { return m_hopOnActive; }
    int mainPower() const { return static_cast<int>(m_mainPower); }
    QList<int> faults() const { return m_faults.values(); }

    // Helper getters for QML
    Q_INVOKABLE bool isParked() const { return m_state == ScootEnums::VehicleState::Parked; }
    Q_INVOKABLE bool isReadyToDrive() const { return m_state == ScootEnums::VehicleState::ReadyToDrive; }
    Q_INVOKABLE bool isOff() const { return m_state == ScootEnums::VehicleState::Off; }
    Q_INVOKABLE bool isShuttingDown() const { return m_state == ScootEnums::VehicleState::ShuttingDown; }
    Q_INVOKABLE bool isStandBy() const { return m_state == ScootEnums::VehicleState::StandBy; }

signals:
    void blinkerStateChanged();
    void blinkerSwitchChanged();
    void blinkOpacityChanged();
    void brakeLeftChanged();
    void brakeRightChanged();
    void kickstandChanged();
    void stateChanged();
    void stateRawChanged();
    void handleBarLockSensorChanged();
    void handlebarInLockPositionChanged();
    void seatboxButtonChanged();
    void seatboxLockChanged();
    void hornButtonChanged();
    void isUnableToDriveChanged();
    void hopOnActiveChanged();
    void mainPowerChanged();
    void faultsChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;
    void applySetUpdate(const QString &name, const QStringList &members) override;

private:
    void onButtonEvent(const QString &channel, const QString &message);
    void updateBlinkClock();
    void setBrake(bool isLeft, ScootEnums::Toggle value);

    static constexpr int BRAKE_DEBOUNCE_MS = 20;

    QTimer m_blinkTimer;
    QElapsedTimer m_blinkElapsed;
    qreal m_blinkOpacity = 0.0;
    qint64 m_blinkPhaseOffset = 0; // ms offset into 800ms cycle, reset when start_nanos arrives

    ScootEnums::BlinkerState m_blinkerState = ScootEnums::BlinkerState::Off;
    ScootEnums::BlinkerSwitch m_blinkerSwitch = ScootEnums::BlinkerSwitch::Off;
    ScootEnums::Toggle m_brakeLeft = ScootEnums::Toggle::Off;
    ScootEnums::Toggle m_brakeRight = ScootEnums::Toggle::Off;
    ScootEnums::Toggle m_pendingBrakeLeft = ScootEnums::Toggle::Off;
    ScootEnums::Toggle m_pendingBrakeRight = ScootEnums::Toggle::Off;
    QTimer m_brakeLeftDebounce;
    QTimer m_brakeRightDebounce;
    ScootEnums::Kickstand m_kickstand = ScootEnums::Kickstand::Down;
    ScootEnums::VehicleState m_state = ScootEnums::VehicleState::Unknown;
    QString m_stateRaw;
    ScootEnums::HandleBarLockSensor m_handleBarLockSensor = ScootEnums::HandleBarLockSensor::Unknown;
    bool m_handlebarInLockPosition = false;
    ScootEnums::Toggle m_seatboxButton = ScootEnums::Toggle::Off;
    ScootEnums::SeatboxLock m_seatboxLock = ScootEnums::SeatboxLock::Closed;
    ScootEnums::Toggle m_hornButton = ScootEnums::Toggle::Off;
    ScootEnums::Toggle m_isUnableToDrive = ScootEnums::Toggle::Off;
    bool m_hopOnActive = false;
    ScootEnums::Toggle m_mainPower = ScootEnums::Toggle::Off;
    QSet<int> m_faults;
};

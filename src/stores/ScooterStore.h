#pragma once

#include <QElapsedTimer>

#include "SyncableStore.h"

class ScooterStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(bool hasTemperature READ hasTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(bool isCold READ isCold NOTIFY frostChanged)
    Q_PROPERTY(bool isFrostWarning READ isFrostWarning NOTIFY frostChanged)

public:
    explicit ScooterStore(MdbRepository *repo, QObject *parent = nullptr);

    double temperature() const { return m_temperature; }
    bool hasTemperature() const { return m_hasTemperature; }
    bool isCold() const { return m_isCold; }
    bool isFrostWarning() const { return m_isFrostWarning; }

signals:
    void temperatureChanged();
    void frostChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    void updateFrostState();
    void updateDisplayedTemperature();

    // Asymmetric hysteresis: easy to enter the cold-warning state, harder to
    // leave it. The exit thresholds sit 2 degC above the entry thresholds so
    // a sensor jittering around the boundary doesn't make the indicator
    // flip-flop, and so we err on the side of warning the rider when in doubt.
    static constexpr double ColdEnter = 10.0;
    static constexpr double ColdExit = 12.0;
    static constexpr double FrostWarningEnter = 5.0;
    static constexpr double FrostWarningExit = 7.0;

    // The dashboard sensor is noisy: standing in the sun vs. moving can swing
    // the reading by a few degrees within seconds. Hold the displayed value
    // unless the raw reading drifts >= DisplayDeadband from it, or the value
    // has been held for >= DisplayHoldMs (so small drift catches up). Bypass
    // the deadband whenever raw is in or near the cold band, so we never lag
    // a frost warning. Frost/cold state itself runs off the raw value and
    // already has its own hysteresis.
    static constexpr double DisplayDeadband = 1.5;
    static constexpr qint64 DisplayHoldMs = 10000;
    static constexpr double DisplayBypassBelow = ColdExit;

    double m_rawTemperature = 0.0;
    double m_temperature = 0.0;
    bool m_hasTemperature = false;
    bool m_isCold = false;
    bool m_isFrostWarning = false;
    QElapsedTimer m_displayHeldSince;
};

#pragma once

#include "SyncableStore.h"

class ScooterStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(bool hasTemperature READ hasTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(bool isFrost READ isFrost NOTIFY frostChanged)
    Q_PROPERTY(bool isFrostWarning READ isFrostWarning NOTIFY frostChanged)

public:
    explicit ScooterStore(MdbRepository *repo, QObject *parent = nullptr);

    double temperature() const { return m_temperature; }
    bool hasTemperature() const { return m_hasTemperature; }
    bool isFrost() const { return m_isFrost; }
    bool isFrostWarning() const { return m_isFrostWarning; }

signals:
    void temperatureChanged();
    void frostChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    void updateFrostState();

    // Asymmetric hysteresis: easy to enter the cold-warning state, harder to
    // leave it. The exit thresholds sit 2 degC above the entry thresholds so
    // a sensor jittering around the boundary doesn't make the indicator
    // flip-flop, and so we err on the side of warning the rider when in doubt.
    static constexpr double FrostEnter = 10.0;
    static constexpr double FrostExit = 12.0;
    static constexpr double FrostWarningEnter = 5.0;
    static constexpr double FrostWarningExit = 7.0;

    double m_temperature = 0.0;
    bool m_hasTemperature = false;
    bool m_isFrost = false;
    bool m_isFrostWarning = false;
};

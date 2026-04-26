#pragma once

#include "SyncableStore.h"

class ScooterStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(bool hasTemperature READ hasTemperature NOTIFY temperatureChanged)

public:
    explicit ScooterStore(MdbRepository *repo, QObject *parent = nullptr);

    double temperature() const { return m_temperature; }
    bool hasTemperature() const { return m_hasTemperature; }

signals:
    void temperatureChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    double m_temperature = 0.0;
    bool m_hasTemperature = false;
};

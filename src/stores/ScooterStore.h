#pragma once

#include <QElapsedTimer>

#include "SyncableStore.h"
#include <QtQml/qqmlengine.h>

class ScooterStore : public SyncableStore
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(bool hasTemperature READ hasTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(bool isFrostWarning READ isFrostWarning NOTIFY frostChanged)

public:
    explicit ScooterStore(MdbRepository *repo, QObject *parent = nullptr);

    double temperature() const { return m_temperature; }
    bool hasTemperature() const { return m_hasTemperature; }
    bool isFrostWarning() const { return m_isFrostWarning; }

signals:
    void temperatureChanged();
    void frostChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    void updateFrostState();

    // Asymmetric hysteresis on the frost warning: enter at <5 degC, exit at
    // >=7 degC. Erring on the side of warning the rider when the smoothed
    // value sits in the boundary band.
    static constexpr double FrostWarningEnter = 5.0;
    static constexpr double FrostWarningExit = 7.0;

    // The dashboard sensor is noisy: sun-on-housing vs. airflow-while-rolling
    // can swing the raw reading by a few degC within seconds, which is sensor
    // artefact, not actual ambient. Smooth raw with an EMA (~60 s tau) and
    // drive both the displayed value and the frost-warning state from the
    // smoothed value, so they move together and neither flickers on jitter.
    // Real outdoor ambient changes far slower than tau, so the warning can't
    // realistically miss a frost approach.
    static constexpr double SmoothingTauSec = 60.0;

    double m_temperature = 0.0;
    bool m_hasTemperature = false;
    bool m_isFrostWarning = false;
    QElapsedTimer m_smoothingClock;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static ScooterStore *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(ScooterStore *instance) { s_qmlInstance = instance; }

private:
    static inline ScooterStore *s_qmlInstance = nullptr;
};

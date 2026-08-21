#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"
#include <QtQml/qqmlengine.h>

class AuxBatteryStore : public SyncableStore
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int dateStreamEnable READ dateStreamEnable NOTIFY dateStreamEnableChanged)
    Q_PROPERTY(int voltage READ voltage NOTIFY voltageChanged)
    Q_PROPERTY(int charge READ charge NOTIFY chargeChanged)
    Q_PROPERTY(int chargeStatus READ chargeStatus NOTIFY chargeStatusChanged)
    // Whether a value has actually been reported, so consumers can tell a real
    // reported 0 from "never received" (the defaults below are not real data).
    Q_PROPERTY(bool voltageValid READ voltageValid NOTIFY voltageValidChanged)
    Q_PROPERTY(bool chargeValid READ chargeValid NOTIFY chargeValidChanged)

public:
    explicit AuxBatteryStore(MdbRepository *repo, QObject *parent = nullptr);

    int dateStreamEnable() const { return m_dateStreamEnable; }
    int voltage() const { return m_voltage; }
    int charge() const { return m_charge; }
    int chargeStatus() const { return static_cast<int>(m_chargeStatus); }
    bool voltageValid() const { return m_voltageValid; }
    bool chargeValid() const { return m_chargeValid; }

signals:
    void dateStreamEnableChanged();
    void voltageChanged();
    void chargeChanged();
    void chargeStatusChanged();
    void voltageValidChanged();
    void chargeValidChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    int m_dateStreamEnable = 0;
    int m_voltage = 12500;
    int m_charge = 100;
    bool m_voltageValid = false;
    bool m_chargeValid = false;
    ScootEnums::AuxChargeStatus m_chargeStatus = ScootEnums::AuxChargeStatus::FloatCharge;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static AuxBatteryStore *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(AuxBatteryStore *instance) { s_qmlInstance = instance; }

private:
    static inline AuxBatteryStore *s_qmlInstance = nullptr;
};

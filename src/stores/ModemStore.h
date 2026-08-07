#pragma once

#include "SyncableStore.h"

// Mirrors the `modem` hash published by modem-service. The identity fields
// (IMEI/ICCID/IMSI) live on the `internet` hash and stay in InternetStore;
// this store carries the network and SIM state around them.
class ModemStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QString powerState READ powerState NOTIFY powerStateChanged)
    Q_PROPERTY(QString simState READ simState NOTIFY simStateChanged)
    Q_PROPERTY(QString simLock READ simLock NOTIFY simLockChanged)
    Q_PROPERTY(QString operatorName READ operatorName NOTIFY operatorNameChanged)
    Q_PROPERTY(QString operatorCode READ operatorCode NOTIFY operatorCodeChanged)
    Q_PROPERTY(QString registration READ registration NOTIFY registrationChanged)
    Q_PROPERTY(QString registrationFail READ registrationFail NOTIFY registrationFailChanged)
    Q_PROPERTY(bool isRoaming READ isRoaming NOTIFY isRoamingChanged)
    Q_PROPERTY(QString errorState READ errorState NOTIFY errorStateChanged)
    Q_PROPERTY(QString pinAction READ pinAction NOTIFY pinActionChanged)
    Q_PROPERTY(QString apnAction READ apnAction NOTIFY apnActionChanged)

public:
    explicit ModemStore(MdbRepository *repo, QObject *parent = nullptr);

    // "on"/"off"
    QString powerState() const { return m_powerState; }
    // "present"/"missing"/"locked"/"inactive"
    QString simState() const { return m_simState; }
    // Unlock required type, empty when nothing is locked.
    QString simLock() const { return m_simLock; }
    QString operatorName() const { return m_operatorName; }
    // MCC+MNC, e.g. "26202".
    QString operatorCode() const { return m_operatorCode; }
    // "home"/"roaming"/"searching"/"denied"/"idle"/"unknown"
    QString registration() const { return m_registration; }
    QString registrationFail() const { return m_registrationFail; }
    bool isRoaming() const { return m_isRoaming; }
    // Consolidated error verdict, "ok" when healthy.
    QString errorState() const { return m_errorState; }
    // Outcome of the last SIM PIN reconcile.
    QString pinAction() const { return m_pinAction; }
    // Outcome of the last APN reconcile.
    QString apnAction() const { return m_apnAction; }

signals:
    void powerStateChanged();
    void simStateChanged();
    void simLockChanged();
    void operatorNameChanged();
    void operatorCodeChanged();
    void registrationChanged();
    void registrationFailChanged();
    void isRoamingChanged();
    void errorStateChanged();
    void pinActionChanged();
    void apnActionChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_powerState;
    QString m_simState;
    QString m_simLock;
    QString m_operatorName;
    QString m_operatorCode;
    QString m_registration;
    QString m_registrationFail;
    bool m_isRoaming = false;
    QString m_errorState;
    QString m_pinAction;
    QString m_apnAction;
};

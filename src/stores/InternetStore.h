#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"

class InternetStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(int modemState READ modemState NOTIFY modemStateChanged)
    Q_PROPERTY(int unuCloud READ unuCloud NOTIFY unuCloudChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY ipAddressChanged)
    Q_PROPERTY(QString accessTech READ accessTech NOTIFY accessTechChanged)
    Q_PROPERTY(int signalQuality READ signalQuality NOTIFY signalQualityChanged)
    Q_PROPERTY(QString simImei READ simImei NOTIFY simImeiChanged)
    Q_PROPERTY(QString simImsi READ simImsi NOTIFY simImsiChanged)
    Q_PROPERTY(QString simIccid READ simIccid NOTIFY simIccidChanged)

public:
    explicit InternetStore(MdbRepository *repo, QObject *parent = nullptr);

    int modemState() const { return static_cast<int>(m_modemState); }
    int unuCloud() const { return static_cast<int>(m_unuCloud); }
    int status() const { return static_cast<int>(m_status); }
    QString ipAddress() const { return m_ipAddress; }
    QString accessTech() const { return m_accessTech; }
    int signalQuality() const { return m_signalQuality; }
    QString simImei() const { return m_simImei; }
    QString simImsi() const { return m_simImsi; }
    QString simIccid() const { return m_simIccid; }

signals:
    void modemStateChanged();
    void unuCloudChanged();
    void statusChanged();
    void ipAddressChanged();
    void accessTechChanged();
    void signalQualityChanged();
    void simImeiChanged();
    void simImsiChanged();
    void simIccidChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    ScootEnums::ModemState m_modemState = ScootEnums::ModemState::Off;
    ScootEnums::ConnectionStatus m_unuCloud = ScootEnums::ConnectionStatus::Disconnected;
    ScootEnums::ConnectionStatus m_status = ScootEnums::ConnectionStatus::Disconnected;
    QString m_ipAddress;
    QString m_accessTech;
    int m_signalQuality = 0;
    QString m_simImei;
    QString m_simImsi;
    QString m_simIccid;
};

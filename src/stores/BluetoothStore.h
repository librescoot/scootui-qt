#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class BluetoothStore : public SyncableStore
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString macAddress READ macAddress NOTIFY macAddressChanged)
    Q_PROPERTY(QString pinCode READ pinCode NOTIFY pinCodeChanged)
    Q_PROPERTY(QString serviceHealth READ serviceHealth NOTIFY serviceHealthChanged)
    Q_PROPERTY(QString serviceError READ serviceError NOTIFY serviceErrorChanged)
    Q_PROPERTY(QString lastUpdate READ lastUpdate NOTIFY lastUpdateChanged)

public:
    explicit BluetoothStore(MdbRepository *repo, QObject *parent = nullptr);
    static BluetoothStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    int status() const { return static_cast<int>(m_status); }
    QString macAddress() const { return m_macAddress; }
    QString pinCode() const { return m_pinCode; }
    QString serviceHealth() const { return m_serviceHealth; }
    QString serviceError() const { return m_serviceError; }
    QString lastUpdate() const { return m_lastUpdate; }

signals:
    void statusChanged();
    void macAddressChanged();
    void pinCodeChanged();
    void serviceHealthChanged();
    void serviceErrorChanged();
    void lastUpdateChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    ScootEnums::ConnectionStatus m_status = ScootEnums::ConnectionStatus::Disconnected;
    QString m_macAddress;
    QString m_pinCode;
    QString m_serviceHealth;
    QString m_serviceError;
    QString m_lastUpdate;
    static inline BluetoothStore *s_instance = nullptr;
};

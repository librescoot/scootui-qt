#pragma once

#include "SyncableStore.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class NavigationStore : public SyncableStore
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString latitude READ latitude NOTIFY latitudeChanged)
    Q_PROPERTY(QString longitude READ longitude NOTIFY longitudeChanged)
    Q_PROPERTY(QString address READ address NOTIFY addressChanged)
    Q_PROPERTY(QString timestamp READ timestamp NOTIFY timestampChanged)
    Q_PROPERTY(QString destination READ destination NOTIFY destinationChanged)

public:
    explicit NavigationStore(MdbRepository *repo, QObject *parent = nullptr);
    static NavigationStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    QString latitude() const { return m_latitude; }
    QString longitude() const { return m_longitude; }
    QString address() const { return m_address; }
    QString timestamp() const { return m_timestamp; }
    QString destination() const { return m_destination; }

    Q_INVOKABLE void setDestination(const QString &dest);
    Q_INVOKABLE void clearDestination();

signals:
    void latitudeChanged();
    void longitudeChanged();
    void addressChanged();
    void timestampChanged();
    void destinationChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_latitude;
    QString m_longitude;
    QString m_address;
    QString m_timestamp;
    QString m_destination;

    static inline NavigationStore *s_instance = nullptr;
};

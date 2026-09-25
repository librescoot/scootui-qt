#pragma once

#include "SyncableStore.h"

class NavigationStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QString latitude READ latitude NOTIFY latitudeChanged)
    Q_PROPERTY(QString longitude READ longitude NOTIFY longitudeChanged)
    Q_PROPERTY(QString address READ address NOTIFY addressChanged)
    Q_PROPERTY(QString timestamp READ timestamp NOTIFY timestampChanged)
    Q_PROPERTY(QString destination READ destination NOTIFY destinationChanged)
    // Raw JSON list of stops pushed by an external channel (cloud, BLE, CLI).
    // NavigationService owns parsing and normalization; the store is only the
    // Redis mirror so the ingest path can tell an external write from its own
    // echo.
    Q_PROPERTY(QString waypoints READ waypoints NOTIFY waypointsChanged)
    Q_PROPERTY(QString currentStep READ currentStep NOTIFY currentStepChanged)
    Q_PROPERTY(QString plan READ plan NOTIFY planChanged)

public:
    explicit NavigationStore(MdbRepository *repo, QObject *parent = nullptr);

    QString latitude() const { return m_latitude; }
    QString longitude() const { return m_longitude; }
    QString address() const { return m_address; }
    QString timestamp() const { return m_timestamp; }
    QString destination() const { return m_destination; }
    QString waypoints() const { return m_waypoints; }
    QString currentStep() const { return m_currentStep; }
    QString plan() const { return m_plan; }

signals:
    void latitudeChanged();
    void longitudeChanged();
    void addressChanged();
    void timestampChanged();
    void destinationChanged();
    void waypointsChanged();
    void currentStepChanged();
    void planChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_latitude;
    QString m_longitude;
    QString m_address;
    QString m_timestamp;
    QString m_destination;
    QString m_waypoints;
    QString m_currentStep;
    QString m_plan;
};

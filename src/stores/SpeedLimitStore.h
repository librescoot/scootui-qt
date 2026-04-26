#pragma once

#include "SyncableStore.h"

class SpeedLimitStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QString speedLimit READ speedLimit NOTIFY speedLimitChanged)
    Q_PROPERTY(QString roadName READ roadName NOTIFY roadNameChanged)
    // Comma-joined road references ("B 2, B 5"). Empty if the road has no
    // ref(s), or if the data source didn't expose them. RoadNameDisplay
    // renders this in parens after the street name.
    Q_PROPERTY(QString roadRefs READ roadRefs NOTIFY roadRefsChanged)
    Q_PROPERTY(QString roadType READ roadType NOTIFY roadTypeChanged)
    Q_PROPERTY(double roadBearing READ roadBearing NOTIFY roadBearingChanged)

public:
    explicit SpeedLimitStore(MdbRepository *repo, QObject *parent = nullptr);

    QString speedLimit() const { return m_speedLimit; }
    QString roadName() const { return m_roadName; }
    QString roadRefs() const { return m_roadRefs; }
    QString roadType() const { return m_roadType; }
    double roadBearing() const { return m_roadBearing; }

    // Direct setters (used by RoadInfoService for tile-derived data)
    void setSpeedLimitDirect(const QString &value);
    void setRoadNameDirect(const QString &value);
    void setRoadRefsDirect(const QString &value);
    void setRoadTypeDirect(const QString &value);
    void setRoadBearingDirect(double value);

signals:
    void speedLimitChanged();
    void roadNameChanged();
    void roadRefsChanged();
    void roadTypeChanged();
    void roadBearingChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_speedLimit;
    QString m_roadName;
    QString m_roadRefs;
    QString m_roadType;
    double m_roadBearing = -1;
};

#pragma once

#include "SyncableStore.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class SpeedLimitStore : public SyncableStore
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString speedLimit READ speedLimit NOTIFY speedLimitChanged)
    Q_PROPERTY(QString roadName READ roadName NOTIFY roadNameChanged)
    Q_PROPERTY(QString roadType READ roadType NOTIFY roadTypeChanged)
    Q_PROPERTY(double roadBearing READ roadBearing NOTIFY roadBearingChanged)

public:
    explicit SpeedLimitStore(MdbRepository *repo, QObject *parent = nullptr);
    static SpeedLimitStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    QString speedLimit() const { return m_speedLimit; }
    QString roadName() const { return m_roadName; }
    QString roadType() const { return m_roadType; }
    double roadBearing() const { return m_roadBearing; }

    // Direct setters (used by RoadInfoService for tile-derived data)
    void setSpeedLimitDirect(const QString &value);
    void setRoadNameDirect(const QString &value);
    void setRoadTypeDirect(const QString &value);
    void setRoadBearingDirect(double value);

signals:
    void speedLimitChanged();
    void roadNameChanged();
    void roadTypeChanged();
    void roadBearingChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_speedLimit;
    QString m_roadName;
    QString m_roadType;
    double m_roadBearing = -1;

    static inline SpeedLimitStore *s_instance = nullptr;
};

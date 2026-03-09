#pragma once

#include "SyncableStore.h"

class SpeedLimitStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QString speedLimit READ speedLimit NOTIFY speedLimitChanged)
    Q_PROPERTY(QString roadName READ roadName NOTIFY roadNameChanged)
    Q_PROPERTY(QString roadType READ roadType NOTIFY roadTypeChanged)

public:
    explicit SpeedLimitStore(MdbRepository *repo, QObject *parent = nullptr);

    QString speedLimit() const { return m_speedLimit; }
    QString roadName() const { return m_roadName; }
    QString roadType() const { return m_roadType; }

signals:
    void speedLimitChanged();
    void roadNameChanged();
    void roadTypeChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_speedLimit;
    QString m_roadName;
    QString m_roadType;
};

#pragma once

#include "SyncableStore.h"
#include <QElapsedTimer>

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
    Q_PROPERTY(QString roadSignStyle READ roadSignStyle NOTIFY roadSignStyleChanged)
    Q_PROPERTY(double roadBearing READ roadBearing NOTIFY roadBearingChanged)

public:
    explicit SpeedLimitStore(MdbRepository *repo, QObject *parent = nullptr);

    QString speedLimit() const { return m_speedLimit; }
    QString roadName() const { return m_roadName; }
    QString roadRefs() const { return m_roadRefs; }
    QString roadType() const { return m_roadType; }
    QString roadSignStyle() const { return m_roadSignStyle; }
    double roadBearing() const { return m_roadBearing; }

    // Ownership is per field: a later external write (even the same value)
    // must survive expiry of an earlier tile match.
    enum class Source { External, Route, Tile };
    void clearSource(Source source);

    // Direct setters used by RoadInfoService.
    void setSpeedLimitDirect(const QString &value, Source source = Source::Route);
    void setRoadNameDirect(const QString &value, Source source = Source::Route);
    void setRoadRefsDirect(const QString &value, Source source = Source::Route);
    void setRoadTypeDirect(const QString &value, Source source = Source::Route);
    void setRoadNetworksDirect(const QString &value, Source source = Source::Route);
    void setRoadBearingDirect(double value, Source source = Source::Route);

signals:
    void speedLimitChanged();
    void roadNameChanged();
    void roadRefsChanged();
    void roadTypeChanged();
    void roadSignStyleChanged();
    void roadBearingChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    void markDirectUpdate(Source source);
    void updateRoadSignStyle();

    Source m_speedLimitSource = Source::External;
    Source m_roadNameSource = Source::External;
    Source m_roadRefsSource = Source::External;
    Source m_roadTypeSource = Source::External;
    Source m_roadNetworksSource = Source::External;
    Source m_roadBearingSource = Source::External;

    QString m_speedLimit;
    QString m_roadName;
    QString m_roadRefs;
    QString m_roadType;
    QString m_roadNetworks;
    QString m_roadSignStyle;
    double m_roadBearing = -1;
    QElapsedTimer m_directUpdateAge;
    static constexpr int DirectAuthorityHoldMs = 2500;
};

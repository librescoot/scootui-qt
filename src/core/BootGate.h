#pragma once

#include <QObject>

// QML side of the READY gate: Application asks for the cluster to be built
// hidden, Main.qml reports when it exists.
class BootGate : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool warmCluster READ warmCluster NOTIFY warmClusterChanged)
    Q_PROPERTY(bool clusterWarm READ clusterWarm NOTIFY clusterWarmChanged)

public:
    explicit BootGate(QObject *parent = nullptr) : QObject(parent) {}

    bool warmCluster() const { return m_warmCluster; }
    bool clusterWarm() const { return m_clusterWarm; }
    void requestWarmCluster();
    Q_INVOKABLE void clusterWarmed();

signals:
    void warmClusterChanged();
    void clusterWarmChanged();

private:
    bool m_warmCluster = false;
    bool m_clusterWarm = false;
};

#include "BootGate.h"

void BootGate::requestWarmCluster()
{
    if (m_warmCluster)
        return;
    m_warmCluster = true;
    emit warmClusterChanged();
}

void BootGate::clusterWarmed()
{
    if (m_clusterWarm)
        return;
    m_clusterWarm = true;
    emit clusterWarmChanged();
}

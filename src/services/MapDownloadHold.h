#pragma once

#include "models/Enums.h"

#include <QString>

// Which DBC power hold to ask vehicle-service for in a given download status.
// Empty means "release". Installing asks for the longer cap: applying a delta
// rebuilds the map on the dashboard's CPU for minutes, and a power cut there
// restarts that work from the beginning.
// How often an install repeats its hold request. vehicle-service restarts its
// cap on each one, and the cap is several times longer than this.
constexpr int MapInstallHeartbeatMs = 60 * 1000;

inline QString mapDownloadHoldFor(int status)
{
    if (status == static_cast<int>(ScootEnums::MapDownloadStatus::Installing))
        return QStringLiteral("map-install");
    if (status == static_cast<int>(ScootEnums::MapDownloadStatus::Downloading))
        return QStringLiteral("map-download");
    return {};
}

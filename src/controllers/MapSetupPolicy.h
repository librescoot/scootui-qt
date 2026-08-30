#pragma once

#include "models/Enums.h"

// Pure decision logic for the Navigation Setup screen: what the download
// button will actually fetch given the setup mode and component health, when
// it is enabled, and which title/body/button wording applies. The size label
// and the download trigger share willDownload*() so they can't drift.
namespace MapSetupPolicy {

struct Inputs {
    int mode = static_cast<int>(ScootEnums::SetupMode::Both);
    bool mapsOk = false;
    bool routingOk = false;
    bool updateAvailable = false;
    bool hasPartialDisplay = false;
    bool hasPartialRouting = false;
    int downloadStatus = static_cast<int>(ScootEnums::MapDownloadStatus::Idle);
    bool online = false;
    bool hasGps = false;
};

inline bool showDisplayRow(const Inputs &in)
{
    return in.mode == static_cast<int>(ScootEnums::SetupMode::DisplayMaps)
        || in.mode == static_cast<int>(ScootEnums::SetupMode::Both);
}

inline bool showRoutingRow(const Inputs &in)
{
    return in.mode == static_cast<int>(ScootEnums::SetupMode::Routing)
        || in.mode == static_cast<int>(ScootEnums::SetupMode::Both);
}

// Mode DisplayMaps/Routing force a single component; Both fetches whatever is
// missing. If everything is fine but an update is available, refresh
// whichever the mode asks for.
inline bool willDownloadDisplay(const Inputs &in)
{
    return showDisplayRow(in) && (!in.mapsOk || in.updateAvailable);
}

inline bool willDownloadRouting(const Inputs &in)
{
    return showRoutingRow(in) && (!in.routingOk || in.updateAvailable);
}

inline bool willDownloadAnything(const Inputs &in)
{
    return willDownloadDisplay(in) || willDownloadRouting(in);
}

inline bool hasRelevantPartial(const Inputs &in)
{
    if (in.mode == static_cast<int>(ScootEnums::SetupMode::DisplayMaps))
        return in.hasPartialDisplay;
    if (in.mode == static_cast<int>(ScootEnums::SetupMode::Routing))
        return in.hasPartialRouting;
    return in.hasPartialDisplay || in.hasPartialRouting;
}

inline bool canDownload(const Inputs &in)
{
    return in.downloadStatus == static_cast<int>(ScootEnums::MapDownloadStatus::Idle)
        && in.online && in.hasGps && willDownloadAnything(in);
}

enum class ButtonAction { Download, Update, Resume };

inline ButtonAction buttonAction(const Inputs &in)
{
    if (in.updateAvailable)
        return ButtonAction::Update;
    if (hasRelevantPartial(in))
        return ButtonAction::Resume;
    return ButtonAction::Download;
}

enum class Title { Setup, MapsUnavailable, RoutingUnavailable, BothUnavailable };

inline Title title(const Inputs &in)
{
    if (in.mode == static_cast<int>(ScootEnums::SetupMode::DisplayMaps))
        return Title::MapsUnavailable;
    if (in.mode == static_cast<int>(ScootEnums::SetupMode::Routing))
        return Title::RoutingUnavailable;
    if (!in.mapsOk && !in.routingOk)
        return Title::BothUnavailable;
    if (!in.mapsOk)
        return Title::MapsUnavailable;
    if (!in.routingOk)
        return Title::RoutingUnavailable;
    return Title::Setup;
}

// Body text picks the description that matches the actual download. In mode
// Both with only one side missing, avoid the "both packs" phrasing. When
// nothing needs downloading (proactive visit from the Navigation submenu
// with everything installed), it's the "all set" message.
enum class Body { AllSet, Both, DisplayOnly, RoutingOnly };

inline Body body(const Inputs &in)
{
    if (!willDownloadAnything(in))
        return Body::AllSet;
    if (willDownloadDisplay(in) && willDownloadRouting(in))
        return Body::Both;
    if (willDownloadDisplay(in))
        return Body::DisplayOnly;
    return Body::RoutingOnly;
}

}

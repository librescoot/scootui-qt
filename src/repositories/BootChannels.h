#pragma once

#include <QStringList>

// Hashes the dashboard needs before its first frame. tests/BootChannelsTest.cpp
// asserts this list equals what the stores register at runtime.
namespace BootChannels {

inline const QStringList &storeChannels()
{
    static const QStringList list{
        QStringLiteral("aux-battery"),
        QStringLiteral("battery:0"),
        QStringLiteral("battery:1"),
        QStringLiteral("ble"),
        QStringLiteral("cb-battery"),
        QStringLiteral("dashboard"),
        QStringLiteral("engine-ecu"),
        QStringLiteral("gps"),
        QStringLiteral("internet"),
        QStringLiteral("modem"),
        QStringLiteral("motion"),
        QStringLiteral("navigation"),
        QStringLiteral("ota"),
        QStringLiteral("scooter"),
        QStringLiteral("settings"),
        QStringLiteral("speed-limit"),
        QStringLiteral("usb"),
        QStringLiteral("vehicle"),
    };
    return list;
}

inline const QStringList &extraPollChannels()
{
    static const QStringList list{
        QStringLiteral("system"),
        QStringLiteral("version:mdb"),
        QStringLiteral("version:dbc"),
    };
    return list;
}

inline QStringList all()
{
    return storeChannels() + extraPollChannels();
}

} // namespace BootChannels

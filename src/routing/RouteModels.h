#pragma once

#include <QString>
#include <QList>
#include <QVariantList>
#include <cmath>

struct LatLng {
    double latitude = 0;
    double longitude = 0;

    bool isValid() const { return latitude != 0 || longitude != 0; }

    double distanceTo(const LatLng &other) const {
        constexpr double R = 6371000.0; // Earth radius in meters
        double lat1 = latitude * M_PI / 180.0;
        double lat2 = other.latitude * M_PI / 180.0;
        double dLat = (other.latitude - latitude) * M_PI / 180.0;
        double dLon = (other.longitude - longitude) * M_PI / 180.0;

        double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                   std::cos(lat1) * std::cos(lat2) *
                   std::sin(dLon / 2) * std::sin(dLon / 2);
        double c = 2 * std::atan2(std::sqrt(std::max(0.0, a)), std::sqrt(std::max(0.0, 1.0 - a)));
        return R * c;
    }

    bool operator==(const LatLng &o) const {
        return latitude == o.latitude && longitude == o.longitude;
    }
    bool operator!=(const LatLng &o) const { return !(*this == o); }
};

enum class ManeuverType {
    Other = 0,
    KeepStraight,
    KeepLeft,
    KeepRight,
    TurnLeft,
    TurnRight,
    TurnSlightLeft,
    TurnSlightRight,
    TurnSharpLeft,
    TurnSharpRight,
    UTurn,
    UTurnRight,
    ExitLeft,
    ExitRight,
    MergeStraight,
    MergeLeft,
    MergeRight,
    RoundaboutEnter,
    RoundaboutExit,
    Ferry
};

enum class NavigationStatus {
    Idle = 0,
    Calculating,
    Navigating,
    Rerouting,
    Arrived,
    Error
};

struct RouteInstruction {
    ManeuverType type = ManeuverType::Other;
    double distance = 0;       // meters
    double duration = 0;       // seconds
    LatLng location;
    int originalShapeIndex = 0;
    QString streetName;
    QString instructionText;
    QString verbalAlertInstruction;
    QString verbalPreTransitionInstruction;
    QString verbalSuccinctInstruction;
    bool verbalMultiCue = false;
    int roundaboutExitCount = 0;
    double bearingBefore = 0;
    double bearingAfter = 0;

    bool operator==(const RouteInstruction &o) const {
        return type == o.type && originalShapeIndex == o.originalShapeIndex &&
               distance == o.distance && streetName == o.streetName;
    }
    bool operator!=(const RouteInstruction &o) const { return !(*this == o); }
};

struct Route {
    QList<RouteInstruction> instructions;
    QList<LatLng> waypoints;
    double distance = 0;   // total meters
    double duration = 0;   // total seconds

    bool isValid() const { return !waypoints.isEmpty() && !instructions.isEmpty(); }
};

Q_DECLARE_METATYPE(Route)

// Decode Google Polyline Algorithm (precision 6 for Valhalla)
inline QList<LatLng> decodePolyline(const QString &encoded, int precision = 6) {
    QList<LatLng> points;
    if (encoded.isEmpty()) return points;

    double factor = std::pow(10, precision);
    int index = 0;
    int lat = 0, lng = 0;
    const auto bytes = encoded.toLatin1();
    int len = bytes.size();

    while (index < len) {
        int shift = 0, result = 0;
        int b;
        do {
            if (index >= len) return points; // Truncated
            b = bytes[index++] - 63;
            result |= (b & 0x1F) << shift;
            shift += 5;
        } while (b >= 0x20);
        lat += (result & 1) ? ~(result >> 1) : (result >> 1);

        shift = 0; result = 0;
        do {
            if (index >= len) return points; // Truncated
            b = bytes[index++] - 63;
            result |= (b & 0x1F) << shift;
            shift += 5;
        } while (b >= 0x20);
        lng += (result & 1) ? ~(result >> 1) : (result >> 1);

        points.append({static_cast<double>(lat) / factor, static_cast<double>(lng) / factor});
    }
    return points;
}

// Map Valhalla maneuver type code to our ManeuverType
inline ManeuverType mapValhallaType(int type) {
    switch (type) {
    case 7: case 8: case 17: return ManeuverType::KeepStraight;
    case 9:   return ManeuverType::TurnSlightRight;
    case 10:  return ManeuverType::TurnRight;
    case 11:  return ManeuverType::TurnSharpRight;
    case 12:  return ManeuverType::UTurn;
    case 13:  return ManeuverType::UTurnRight;
    case 14:  return ManeuverType::TurnSharpLeft;
    case 15:  return ManeuverType::TurnSlightLeft;
    case 16:  return ManeuverType::TurnLeft;
    case 18:  return ManeuverType::TurnRight;
    case 19:  return ManeuverType::TurnLeft;
    case 20:  return ManeuverType::ExitRight;
    case 21:  return ManeuverType::ExitLeft;
    case 22:  return ManeuverType::KeepStraight;
    case 23:  return ManeuverType::KeepLeft;
    case 24:  return ManeuverType::KeepRight;
    case 25:  return ManeuverType::MergeStraight;
    case 26:  return ManeuverType::RoundaboutEnter;
    case 27:  return ManeuverType::RoundaboutExit;
    case 28: case 29: return ManeuverType::Ferry;
    case 37:  return ManeuverType::MergeLeft;
    case 38:  return ManeuverType::MergeRight;
    default:  return ManeuverType::Other;
    }
}

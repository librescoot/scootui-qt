#pragma once

#include "routing/RouteModels.h"

#include <QString>
#include <QVariantList>

// GeoJSON for the multi-hop plan overlay: the previewed remaining route drawn
// under the active route, plus one marker per stop. Free of MapService so the
// encoding can be unit-tested.
namespace MapPlanGeometry {

// Feature LineString, or an empty FeatureCollection when there is nothing to
// draw. Coordinates are [longitude, latitude].
QString lineGeoJson(const QList<LatLng> &points);

// FeatureCollection of stop Points with properties index, current, reached,
// first, last, label. Accepts NavigationService::planStops().
QString stopsGeoJson(const QVariantList &stops, int currentStep);

// The completed portion of the active route, through the matched segment.
// A valid matchedPosition clips the line within that segment.
QString traveledGeoJson(const QList<LatLng> &route, int segment,
                        const LatLng &matchedPosition);

// Markers displayed only while the extent-aware route overview is open.
QString overviewMarkersGeoJson(const LatLng &start, const LatLng &finish,
                               const LatLng &currentPosition);

} // namespace MapPlanGeometry

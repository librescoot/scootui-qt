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

} // namespace MapPlanGeometry

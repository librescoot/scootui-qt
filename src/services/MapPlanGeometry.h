#pragma once

#include "routing/RouteModels.h"

#include <QString>
#include <QVariantList>

// GeoJSON for the multi-hop plan overlay: the previewed remaining route drawn
// dim and dashed under the active route, plus one marker per stop.
//
// Kept free of MapService so the encoding can be unit-tested on its own.
namespace MapPlanGeometry {

// A single LineString feature through the plan's remaining geometry, or an
// empty FeatureCollection when there is nothing to draw. GeoJSON coordinates
// are [longitude, latitude].
QString lineGeoJson(const QList<LatLng> &points);

// A FeatureCollection with one Point per stop. Each feature carries
// properties: index, current (1 for the stop being guided to), reached, and
// label. Accepts the maps NavigationService::planStops() returns.
QString stopsGeoJson(const QVariantList &stops, int currentStep);

} // namespace MapPlanGeometry

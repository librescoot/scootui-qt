#pragma once

#include <QObject>
#include <QVariantList>
#include "models/SavedLocation.h"

class MdbRepository;
class SavedLocationsService;
class GpsStore;
class RoadInfoService;
class NavigationService;
class ToastService;

class SavedLocationsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList locations READ locations NOTIFY locationsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(int count READ count NOTIFY locationsChanged)

public:
    explicit SavedLocationsStore(MdbRepository *repo,
                                  SavedLocationsService *service,
                                  GpsStore *gps, RoadInfoService *roadInfo,
                                  NavigationService *nav,
                                  ToastService *toast, QObject *parent = nullptr);

    QVariantList locations() const;
    bool isLoading() const { return m_isLoading; }
    int count() const { return m_locations.size(); }

    Q_INVOKABLE void load();
    Q_INVOKABLE void saveCurrentLocation();
    Q_INVOKABLE void deleteLocation(int id);
    Q_INVOKABLE void navigateToLocation(int id);

signals:
    void locationsChanged();
    void isLoadingChanged();

private:
    SavedLocationsService *m_service;
    GpsStore *m_gps;
    RoadInfoService *m_roadInfo;
    NavigationService *m_nav;
    ToastService *m_toast;

    QList<SavedLocation> m_locations;
    bool m_isLoading = false;
};

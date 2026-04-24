#pragma once

#include <QObject>
#include <QVariantList>
#include "models/RecentDestination.h"

class MdbRepository;
class RecentDestinationsService;
class SavedLocationsService;
class NavigationService;
class ToastService;

class RecentDestinationsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList destinations READ destinations NOTIFY destinationsChanged)
    Q_PROPERTY(int count READ count NOTIFY destinationsChanged)

public:
    explicit RecentDestinationsStore(MdbRepository *repo,
                                       RecentDestinationsService *service,
                                       SavedLocationsService *savedService,
                                       NavigationService *nav,
                                       ToastService *toast,
                                       QObject *parent = nullptr);

    QVariantList destinations() const;
    int count() const { return m_destinations.size(); }

    Q_INVOKABLE void load();

    // Called after NavigationService::setDestination — pushes a new entry,
    // dedupes on ~10 m proximity, and evicts the oldest when past MaxRecents.
    Q_INVOKABLE void push(double lat, double lng, const QString &label);

    Q_INVOKABLE void navigateToRecent(int id);
    Q_INVOKABLE void promoteToSaved(int id);
    Q_INVOKABLE void deleteRecent(int id);

signals:
    void destinationsChanged();

private:
    int findExistingWithinProximity(double lat, double lng, double meters = 10.0) const;
    int evictOldestSlot() const;

    RecentDestinationsService *m_service;
    SavedLocationsService *m_savedService;
    NavigationService *m_nav;
    ToastService *m_toast;

    QList<RecentDestination> m_destinations; // newest first
};

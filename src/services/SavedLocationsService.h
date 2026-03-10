#pragma once

#include <QObject>
#include <QList>
#include "models/SavedLocation.h"

class MdbRepository;

class SavedLocationsService : public QObject
{
    Q_OBJECT

public:
    explicit SavedLocationsService(MdbRepository *repo, QObject *parent = nullptr);

    QList<SavedLocation> loadAll() const;
    bool save(const SavedLocation &location);
    bool remove(int id);
    bool updateLastUsed(int id);

    static constexpr int MaxLocations = 10;

private:
    QString fieldKey(int id, const QString &field) const;
    int findFreeSlot() const;

    MdbRepository *m_repo;
};

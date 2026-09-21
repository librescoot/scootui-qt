#pragma once

#include <QObject>
#include <QList>
#include "models/SavedLocation.h"

class MdbRepository;
class DestinationRpc;

// A quick-nav assignment left in the indexed record fields. Seeded into
// dashboard.shortcut-menu.items once; the fields themselves are write-free
// afterward and only cleared when their record goes away.
struct LegacyQuickAssignment {
    int id = -1;
    int slot = 0;
    QString icon;
    QString uuid;
};

// Saved-location records are managed by settings-service: saves, deletions,
// and timestamp touches travel over its destination RPC, which owns slot
// allocation, UUID assignment, and whole-record field sets. Reads stay on the
// settings hash.
class SavedLocationsService : public QObject
{
    Q_OBJECT

public:
    explicit SavedLocationsService(MdbRepository *repo, QObject *parent = nullptr);

    QList<SavedLocation> loadAll();
    bool save(const SavedLocation &location);
    bool remove(int id);
    bool updateLastUsed(int id);
    QList<LegacyQuickAssignment> loadLegacyQuickAssignments() const;

    static constexpr int MaxLocations = 30;

private:
    QString fieldKey(int id, const QString &field) const;

    MdbRepository *m_repo;
    DestinationRpc *m_rpc;
};

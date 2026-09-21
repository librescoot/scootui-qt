#pragma once

#include <QObject>
#include <QList>
#include "models/SavedLocation.h"

class MdbRepository;

// A quick-nav assignment left in the indexed record fields. Seeded into
// dashboard.shortcut-menu.items once; the fields themselves are write-free
// afterward and only cleared when their record goes away.
struct LegacyQuickAssignment {
    int id = -1;
    int slot = 0;
    QString icon;
    QString uuid;
};

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
    QString ensureUuid(int id);
    void pruneDestinationItem(const QString &uuid);
    QString fieldKey(int id, const QString &field) const;
    int findFreeSlot() const;

    MdbRepository *m_repo;
};

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
    bool setQuickSlot(int id, int quickSlot);
    bool setQuickIcon(int id, const QString &quickIcon);

    static constexpr int MaxLocations = 30;

private:
    bool updateQuickMenu(int id, int quickSlot, const QString &quickIcon);
    QString fieldKey(int id, const QString &field) const;
    int findFreeSlot() const;

    MdbRepository *m_repo;
};

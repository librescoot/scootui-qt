#pragma once

#include <QObject>
#include <QList>
#include "models/RecentDestination.h"

class MdbRepository;

class RecentDestinationsService : public QObject
{
    Q_OBJECT

public:
    explicit RecentDestinationsService(MdbRepository *repo, QObject *parent = nullptr);

    QList<RecentDestination> loadAll() const;
    bool save(const RecentDestination &dest);
    bool remove(int id);

    static constexpr int MaxRecents = 10;

private:
    QString fieldKey(int id, const QString &field) const;
    int findFreeSlot(const QList<RecentDestination> &existing) const;

    MdbRepository *m_repo;
};

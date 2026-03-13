#pragma once

#include <QObject>
#include <QVariantList>

#include "../repositories/MdbRepository.h"

// Loads firmware version data from Redis and exposes it for QML display.
class SystemInfoService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList versionRows READ versionRows NOTIFY versionRowsChanged)

public:
    explicit SystemInfoService(MdbRepository *repo, QObject *parent = nullptr);

    QVariantList versionRows() const { return m_versionRows; }

    // Call once repository is ready to fetch version data
    void loadVersions();

signals:
    void versionRowsChanged();

private:
    MdbRepository *m_repo;
    QVariantList m_versionRows; // [{label, value}, ...]
};

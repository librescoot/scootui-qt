#pragma once

#include <QObject>
#include <QVariantList>

#include "../repositories/MdbRepository.h"

// Loads firmware version data from Redis and exposes it for QML display.
class SystemInfoService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList versionRows READ versionRows NOTIFY versionRowsChanged)
    Q_PROPERTY(QString mdbVersion READ mdbVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString dbcVersion READ dbcVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString nrfVersion READ nrfVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString ecuVersion READ ecuVersion NOTIFY versionRowsChanged)

public:
    explicit SystemInfoService(MdbRepository *repo, QObject *parent = nullptr);

    QVariantList versionRows() const { return m_versionRows; }
    QString mdbVersion() const { return m_mdbVersion; }
    QString dbcVersion() const { return m_dbcVersion; }
    QString nrfVersion() const { return m_nrfVersion; }
    QString ecuVersion() const { return m_ecuVersion; }

    // Call once repository is ready to fetch version data
    Q_INVOKABLE void loadVersions();

signals:
    void versionRowsChanged();

private:
    MdbRepository *m_repo;
    QVariantList m_versionRows; // [{label, value}, ...]
    QString m_mdbVersion;
    QString m_dbcVersion;
    QString m_nrfVersion;
    QString m_ecuVersion;
};

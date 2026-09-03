#pragma once

#include <QObject>
#include <QVariantList>

#include "../repositories/MdbRepository.h"

// Loads firmware version data from Redis and exposes it for QML display.
class SystemInfoService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList versionRows READ versionRows NOTIFY versionRowsChanged)
    Q_PROPERTY(QVariantList mdbBoardRows READ mdbBoardRows NOTIFY versionRowsChanged)
    Q_PROPERTY(QVariantList dbcBoardRows READ dbcBoardRows NOTIFY versionRowsChanged)
    Q_PROPERTY(QVariantList nrfBoardRows READ nrfBoardRows NOTIFY versionRowsChanged)
    Q_PROPERTY(QVariantList ecuBoardRows READ ecuBoardRows NOTIFY versionRowsChanged)
    Q_PROPERTY(QString mdbVersion READ mdbVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString mdbVersionId READ mdbVersionId NOTIFY versionRowsChanged)
    Q_PROPERTY(QString dbcVersion READ dbcVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString nrfVersion READ nrfVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString ecuVersion READ ecuVersion NOTIFY versionRowsChanged)

public:
    explicit SystemInfoService(MdbRepository *repo, QObject *parent = nullptr);

    QVariantList versionRows() const { return m_versionRows; }
    // Per-board identity blocks for the System > Info device page: one row
    // list each for MDB, DBC, nRF and ECU. Rows carry a translation key rather
    // than a label, and are omitted when the underlying field is absent.
    QVariantList mdbBoardRows() const { return m_mdbBoardRows; }
    QVariantList dbcBoardRows() const { return m_dbcBoardRows; }
    QVariantList nrfBoardRows() const { return m_nrfBoardRows; }
    QVariantList ecuBoardRows() const { return m_ecuBoardRows; }
    QString mdbVersion() const { return m_mdbVersion; }
    // The raw VERSION_ID, not the display string: it is what the release tags
    // are built from, so it is the one channel inference can be run against.
    QString mdbVersionId() const { return m_mdbVersionId; }
    QString dbcVersion() const { return m_dbcVersion; }
    QString nrfVersion() const { return m_nrfVersion; }
    QString ecuVersion() const { return m_ecuVersion; }

    // Call once repository is ready to fetch version data
    Q_INVOKABLE void loadVersions();

signals:
    void versionRowsChanged();

private:
    void recomputeVersions();

    MdbRepository *m_repo;
    QVariantList m_versionRows; // [{label, value}, ...]
    QVariantList m_mdbBoardRows;
    QVariantList m_dbcBoardRows;
    QVariantList m_nrfBoardRows;
    QVariantList m_ecuBoardRows;
    QString m_mdbVersion;
    QString m_mdbVersionId;
    QString m_dbcVersion;
    QString m_nrfVersion;
    QString m_ecuVersion;
};

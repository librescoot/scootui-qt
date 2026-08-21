#pragma once

#include <QObject>
#include <QVariantList>

#include "../repositories/MdbRepository.h"
#include <QtQml/qqmlengine.h>

// Loads firmware version data from Redis and exposes it for QML display.
class SystemInfoService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList versionRows READ versionRows NOTIFY versionRowsChanged)
    Q_PROPERTY(QVariantList deviceRows READ deviceRows NOTIFY versionRowsChanged)
    Q_PROPERTY(QString mdbVersion READ mdbVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString mdbVersionId READ mdbVersionId NOTIFY versionRowsChanged)
    Q_PROPERTY(QString dbcVersion READ dbcVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString nrfVersion READ nrfVersion NOTIFY versionRowsChanged)
    Q_PROPERTY(QString ecuVersion READ ecuVersion NOTIFY versionRowsChanged)

public:
    explicit SystemInfoService(MdbRepository *repo, QObject *parent = nullptr);

    QVariantList versionRows() const { return m_versionRows; }
    // Board identity from the `system` hash: flavor, environment and the OCOTP
    // serials. Rows are omitted when the underlying field is absent, which is
    // normal: nothing currently populates the MDB serials on every unit.
    QVariantList deviceRows() const { return m_deviceRows; }
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
    QVariantList m_deviceRows;  // [{label, value}, ...]
    QString m_mdbVersion;
    QString m_mdbVersionId;
    QString m_dbcVersion;
    QString m_nrfVersion;
    QString m_ecuVersion;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static SystemInfoService *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(SystemInfoService *instance) { s_qmlInstance = instance; }

private:
    static inline SystemInfoService *s_qmlInstance = nullptr;
};

#include "SystemInfoService.h"

#include <QVariantMap>

SystemInfoService::SystemInfoService(MdbRepository *repo, QObject *parent)
    : QObject(parent), m_repo(repo)
{
    connect(m_repo, &MdbRepository::fieldsUpdated,
            this, [this](const QString &channel, const FieldMap &) {
        if (channel == QLatin1String("system")
            || channel == QLatin1String("version:mdb")
            || channel == QLatin1String("version:dbc")
            || channel == QLatin1String("engine-ecu")) {
            recomputeVersions();
        }
    });
}

void SystemInfoService::loadVersions()
{
    // Force an immediate refresh so the AboutScreen doesn't have to wait up to
    // 30s for the next scheduled poll. Results arrive asynchronously and trigger
    // recomputeVersions() via the fieldsUpdated subscription.
    m_repo->requestAll(QStringLiteral("system"));
    m_repo->requestAll(QStringLiteral("version:mdb"));
    m_repo->requestAll(QStringLiteral("version:dbc"));
    m_repo->requestAll(QStringLiteral("engine-ecu"));

    recomputeVersions();
}

void SystemInfoService::recomputeVersions()
{
    FieldMap system = m_repo->getAll(QStringLiteral("system"));
    FieldMap mdbVer = m_repo->getAll(QStringLiteral("version:mdb"));
    FieldMap dbcVer = m_repo->getAll(QStringLiteral("version:dbc"));
    FieldMap engineEcu = m_repo->getAll(QStringLiteral("engine-ecu"));

    QVariantList rows;

    auto addRow = [&rows](const QString &label, const QString &value) {
        QVariantMap row;
        row[QStringLiteral("label")] = label + QLatin1Char(':');
        row[QStringLiteral("value")] = value.isEmpty() ? QStringLiteral("\u2014") : value;
        rows.append(row);
    };

    if (mdbVer.contains(QStringLiteral("version"))) {
        addRow(QStringLiteral("MDB"), mdbVer.value(QStringLiteral("version")));
    } else if (system.contains(QStringLiteral("mdb-version"))) {
        addRow(QStringLiteral("MDB"), system[QStringLiteral("mdb-version")]);
    }

    if (dbcVer.contains(QStringLiteral("version"))) {
        addRow(QStringLiteral("DBC"), dbcVer.value(QStringLiteral("version")));
    } else if (system.contains(QStringLiteral("dbc-version"))) {
        addRow(QStringLiteral("DBC"), system[QStringLiteral("dbc-version")]);
    }

    const QString nrf = system.value(QStringLiteral("nrf-fw-version"));
    if (!nrf.isEmpty()) addRow(QStringLiteral("nRF"), nrf);

    const QString ecu = engineEcu.value(QStringLiteral("fw-version"));
    if (!ecu.isEmpty()) addRow(QStringLiteral("ECU"), ecu);

    // Individual version strings for direct QML binding
    auto ver = [](const QString &v) { return v.isEmpty() ? QStringLiteral("unknown") : v; };

    QString mdbVersion = mdbVer.contains(QStringLiteral("version"))
        ? ver(mdbVer.value(QStringLiteral("version")))
        : ver(system.value(QStringLiteral("mdb-version")));

    QString dbcVersion = dbcVer.contains(QStringLiteral("version"))
        ? ver(dbcVer.value(QStringLiteral("version")))
        : ver(system.value(QStringLiteral("dbc-version")));

    QString nrfVersion = ver(nrf);
    QString ecuVersion = ver(ecu);

    if (rows == m_versionRows
        && mdbVersion == m_mdbVersion
        && dbcVersion == m_dbcVersion
        && nrfVersion == m_nrfVersion
        && ecuVersion == m_ecuVersion) {
        return;
    }

    m_versionRows = rows;
    m_mdbVersion = mdbVersion;
    m_dbcVersion = dbcVersion;
    m_nrfVersion = nrfVersion;
    m_ecuVersion = ecuVersion;
    emit versionRowsChanged();
}

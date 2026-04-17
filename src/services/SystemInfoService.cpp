#include "SystemInfoService.h"

#include <QVariantMap>

SystemInfoService::SystemInfoService(MdbRepository *repo, QObject *parent)
    : QObject(parent), m_repo(repo)
{
    s_instance = this;
}

void SystemInfoService::loadVersions()
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

    if (mdbVer.contains(QStringLiteral("version")))
        m_mdbVersion = ver(mdbVer.value(QStringLiteral("version")));
    else
        m_mdbVersion = ver(system.value(QStringLiteral("mdb-version")));

    if (dbcVer.contains(QStringLiteral("version")))
        m_dbcVersion = ver(dbcVer.value(QStringLiteral("version")));
    else
        m_dbcVersion = ver(system.value(QStringLiteral("dbc-version")));

    m_nrfVersion = ver(nrf);
    m_ecuVersion = ver(ecu);

    m_versionRows = rows;
    emit versionRowsChanged();
}

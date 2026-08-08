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

// Picks the human-readable version from a version:<board> hash, falling back
// to the lowercase version_id when the full string is absent.
static QString boardVersion(const FieldMap &ver)
{
    const QString v = ver.value(QStringLiteral("version"));
    return v.isEmpty() ? ver.value(QStringLiteral("version_id")) : v;
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

    QVariantList deviceRows;

    // Board identity. Rows carry a translation key rather than a label, since
    // the screen renders them in the rider's language. Absent fields are
    // dropped rather than shown empty, since a row of placeholders reads as
    // breakage rather than as "not reported here".
    auto addDeviceRow = [&deviceRows](const QString &key, const QString &value) {
        if (value.isEmpty())
            return;
        QVariantMap row;
        row[QStringLiteral("key")] = key;
        row[QStringLiteral("value")] = value;
        deviceRows.append(row);
    };

    // Board serials live on the per-board version hashes, written by
    // version-service from the i.MX6 OCOTP fuses: serial_number_real is the
    // full UID, serial_number the decimal sum of CFG0 and CFG1. Prefer the
    // full one. They are not on the `system` hash, whatever a `*-sn` field
    // there may look like.
    auto serial = [](const FieldMap &ver) {
        const QString real = ver.value(QStringLiteral("serial_number_real"));
        return real.isEmpty() ? ver.value(QStringLiteral("serial_number")) : real;
    };

    addDeviceRow(QStringLiteral("infoMdbSerial"), serial(mdbVer));
    addDeviceRow(QStringLiteral("infoDbcSerial"), serial(dbcVer));

    // Board versions come from version-service only. The system hash carries
    // mdb-version/dbc-version too, but radio-gaga writes those, and the
    // dashboard must not depend on a cloud client being installed.
    const QString mdbV = boardVersion(mdbVer);
    if (!mdbV.isEmpty()) addRow(QStringLiteral("MDB"), mdbV);

    const QString dbcV = boardVersion(dbcVer);
    if (!dbcV.isEmpty()) addRow(QStringLiteral("DBC"), dbcV);

    const QString nrf = system.value(QStringLiteral("nrf-fw-version"));
    if (!nrf.isEmpty()) addRow(QStringLiteral("nRF"), nrf);

    const QString ecu = engineEcu.value(QStringLiteral("fw-version"));
    if (!ecu.isEmpty()) addRow(QStringLiteral("ECU"), ecu);

    // Individual version strings for direct QML binding
    auto ver = [](const QString &v) { return v.isEmpty() ? QStringLiteral("unknown") : v; };

    QString mdbVersion = ver(mdbV);
    QString dbcVersion = ver(dbcV);

    QString nrfVersion = ver(nrf);
    QString ecuVersion = ver(ecu);

    if (rows == m_versionRows
        && deviceRows == m_deviceRows
        && mdbVersion == m_mdbVersion
        && dbcVersion == m_dbcVersion
        && nrfVersion == m_nrfVersion
        && ecuVersion == m_ecuVersion) {
        return;
    }

    m_versionRows = rows;
    m_deviceRows = deviceRows;
    m_mdbVersion = mdbVersion;
    m_dbcVersion = dbcVersion;
    m_nrfVersion = nrfVersion;
    m_ecuVersion = ecuVersion;
    emit versionRowsChanged();
}

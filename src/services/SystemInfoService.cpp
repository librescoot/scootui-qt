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
            || channel == QLatin1String("engine-ecu")
            || channel == QLatin1String("ble")) {
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
    m_repo->requestAll(QStringLiteral("ble"));

    recomputeVersions();
}

// Picks the human-readable version from a version:<board> hash, falling back
// to the lowercase version_id when the full string is absent.
static QString boardVersion(const FieldMap &ver)
{
    const QString v = ver.value(QStringLiteral("version"));
    return v.isEmpty() ? ver.value(QStringLiteral("version_id")) : v;
}

// "1000000" → "1 Mbit/s", "500000" → "500 kbit/s"; empty when absent or not
// a number, so the row gets dropped upstream.
static QString decodeLinkBaud(const FieldMap &ble)
{
    bool ok = false;
    const qint64 baud = ble.value(QStringLiteral("link-baud")).toLongLong(&ok);
    if (!ok || baud <= 0)
        return QString();
    if (baud % 1000000 == 0)
        return QString::number(baud / 1000000) + QLatin1String(" Mbit/s");
    return QString::number(baud / 1000) + QLatin1String(" kbit/s");
}

// Capability bitmask from bluetooth-service: bit 0 = 1 Mbaud operation,
// bit 1 = OTA tunnel forwarding. Unknown bits are ignored, and an all-zero
// or absent mask yields an empty string so the row gets dropped.
static QString decodeLinkCaps(const FieldMap &ble)
{
    bool ok = false;
    const int caps = ble.value(QStringLiteral("link-caps")).toInt(&ok);
    if (!ok || caps <= 0)
        return QString();
    QStringList decoded;
    if (caps & 0x01)
        decoded << QLatin1String("1 Mbit/s");
    if (caps & 0x02)
        decoded << QLatin1String("OTA tunnel");
    return decoded.join(QLatin1String(" \u00b7 "));
}

void SystemInfoService::recomputeVersions()
{
    FieldMap system = m_repo->getAll(QStringLiteral("system"));
    FieldMap mdbVer = m_repo->getAll(QStringLiteral("version:mdb"));
    FieldMap dbcVer = m_repo->getAll(QStringLiteral("version:dbc"));
    FieldMap engineEcu = m_repo->getAll(QStringLiteral("engine-ecu"));
    FieldMap ble = m_repo->getAll(QStringLiteral("ble"));

    QVariantList rows;

    auto addRow = [&rows](const QString &label, const QString &value) {
        QVariantMap row;
        row[QStringLiteral("label")] = label + QLatin1Char(':');
        row[QStringLiteral("value")] = value.isEmpty() ? QStringLiteral("\u2014") : value;
        rows.append(row);
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

    // Per-board blocks for the device page. Rows carry a translation key
    // rather than a label, since the screen renders them in the rider's
    // language. Absent fields are dropped rather than shown empty, since a
    // row of placeholders reads as breakage rather than as "not reported here".
    auto addBoardRow = [](QVariantList &board, const QString &key, const QString &value) {
        if (value.isEmpty())
            return;
        QVariantMap row;
        row[QStringLiteral("key")] = key;
        row[QStringLiteral("value")] = value;
        board.append(row);
    };

    QVariantList mdbRows, dbcRows, nrfRows, ecuRows;

    addBoardRow(mdbRows, QStringLiteral("infoFirmware"), boardVersion(mdbVer));
    addBoardRow(mdbRows, QStringLiteral("infoSerial"), serial(mdbVer));

    addBoardRow(dbcRows, QStringLiteral("infoFirmware"), boardVersion(dbcVer));
    addBoardRow(dbcRows, QStringLiteral("infoSerial"), serial(dbcVer));

    addBoardRow(nrfRows, QStringLiteral("infoFirmware"), system.value(QStringLiteral("nrf-fw-version")));
    addBoardRow(nrfRows, QStringLiteral("infoLinkBaud"), decodeLinkBaud(ble));
    addBoardRow(nrfRows, QStringLiteral("infoLinkCaps"), decodeLinkCaps(ble));
    addBoardRow(nrfRows, QStringLiteral("infoDfuStatus"), ble.value(QStringLiteral("firmware-update-status")));

    // MAC and connection state are on the Connectivity page (BLUETOOTH section)
    // and are deliberately not repeated here. The ECU's live telemetry belongs
    // to the cluster and faults screens; only its identity and nameplate
    // rating land here.
    addBoardRow(ecuRows, QStringLiteral("infoFirmware"), engineEcu.value(QStringLiteral("fw-version")));
    addBoardRow(ecuRows, QStringLiteral("infoFwBase"), engineEcu.value(QStringLiteral("fw:base-version")));
    addBoardRow(ecuRows, QStringLiteral("infoFwApp"), engineEcu.value(QStringLiteral("fw:app-version")));

    bool ok = false;
    const double ratedKw = engineEcu.value(QStringLiteral("motor:rated-power-kw")).toDouble(&ok);
    if (ok && ratedKw > 0)
        addBoardRow(ecuRows, QStringLiteral("infoRatedPower"),
                    QString::number(ratedKw, 'g', 3) + QLatin1String(" kW"));
    const double maxSpeed = engineEcu.value(QStringLiteral("motor:max-speed-kmh")).toDouble(&ok);
    if (ok && maxSpeed > 0)
        addBoardRow(ecuRows, QStringLiteral("infoMaxSpeed"),
                    QString::number(maxSpeed, 'g', 3) + QLatin1String(" km/h"));

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
    const QString mdbVersionId = mdbVer.value(QStringLiteral("version_id"));
    QString dbcVersion = ver(dbcV);

    QString nrfVersion = ver(nrf);
    QString ecuVersion = ver(ecu);

    if (rows == m_versionRows
        && mdbRows == m_mdbBoardRows
        && dbcRows == m_dbcBoardRows
        && nrfRows == m_nrfBoardRows
        && ecuRows == m_ecuBoardRows
        && mdbVersion == m_mdbVersion
        && mdbVersionId == m_mdbVersionId
        && dbcVersion == m_dbcVersion
        && nrfVersion == m_nrfVersion
        && ecuVersion == m_ecuVersion) {
        return;
    }

    m_versionRows = rows;
    m_mdbBoardRows = mdbRows;
    m_dbcBoardRows = dbcRows;
    m_nrfBoardRows = nrfRows;
    m_ecuBoardRows = ecuRows;
    m_mdbVersion = mdbVersion;
    m_mdbVersionId = mdbVersionId;
    m_dbcVersion = dbcVersion;
    m_nrfVersion = nrfVersion;
    m_ecuVersion = ecuVersion;
    emit versionRowsChanged();
}

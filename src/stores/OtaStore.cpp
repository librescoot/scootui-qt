#include "OtaStore.h"

OtaStore::OtaStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

bool OtaStore::isActive() const
{
    return m_dbcStatus != QLatin1String("idle") || m_mdbStatus != QLatin1String("idle");
}

void OtaStore::setBacklightOff(bool off)
{
    m_repo->set(QStringLiteral("dashboard"),
                QStringLiteral("backlight-off"),
                off ? QStringLiteral("1") : QStringLiteral("0"));
}

SyncSettings OtaStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("ota"), 5000,
        {
            {QStringLiteral("dbcStatus"), QStringLiteral("status:dbc")},
            {QStringLiteral("dbcUpdateVersion"), QStringLiteral("update-version:dbc")},
            {QStringLiteral("dbcUpdateMethod"), QStringLiteral("update-method:dbc")},
            {QStringLiteral("dbcError"), QStringLiteral("error:dbc")},
            {QStringLiteral("dbcErrorMessage"), QStringLiteral("error-message:dbc")},
            {QStringLiteral("dbcDownloadProgress"), QStringLiteral("download-progress:dbc")},
            {QStringLiteral("dbcInstallProgress"), QStringLiteral("install-progress:dbc")},
            {QStringLiteral("mdbStatus"), QStringLiteral("status:mdb")},
            {QStringLiteral("mdbUpdateVersion"), QStringLiteral("update-version:mdb")},
            {QStringLiteral("mdbUpdateMethod"), QStringLiteral("update-method:mdb")},
            {QStringLiteral("mdbError"), QStringLiteral("error:mdb")},
            {QStringLiteral("mdbErrorMessage"), QStringLiteral("error-message:mdb")},
            {QStringLiteral("mdbDownloadProgress"), QStringLiteral("download-progress:mdb")},
            {QStringLiteral("mdbInstallProgress"), QStringLiteral("install-progress:mdb")},
        },
        {}, {}
    };
}

void OtaStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    bool activeChanged = false;

    if (variable == QLatin1String("status:dbc")) {
        if (value != m_dbcStatus) { m_dbcStatus = value; emit dbcStatusChanged(); activeChanged = true; }
    } else if (variable == QLatin1String("update-version:dbc")) {
        if (value != m_dbcUpdateVersion) { m_dbcUpdateVersion = value; emit dbcUpdateVersionChanged(); }
    } else if (variable == QLatin1String("update-method:dbc")) {
        if (value != m_dbcUpdateMethod) { m_dbcUpdateMethod = value; emit dbcUpdateMethodChanged(); }
    } else if (variable == QLatin1String("error:dbc")) {
        if (value != m_dbcError) { m_dbcError = value; emit dbcErrorChanged(); }
    } else if (variable == QLatin1String("error-message:dbc")) {
        if (value != m_dbcErrorMessage) { m_dbcErrorMessage = value; emit dbcErrorMessageChanged(); }
    } else if (variable == QLatin1String("download-progress:dbc")) {
        int v = value.toInt();
        if (v != m_dbcDownloadProgress) { m_dbcDownloadProgress = v; emit dbcDownloadProgressChanged(); }
    } else if (variable == QLatin1String("install-progress:dbc")) {
        int v = value.toInt();
        if (v != m_dbcInstallProgress) { m_dbcInstallProgress = v; emit dbcInstallProgressChanged(); }
    } else if (variable == QLatin1String("status:mdb")) {
        if (value != m_mdbStatus) { m_mdbStatus = value; emit mdbStatusChanged(); activeChanged = true; }
    } else if (variable == QLatin1String("update-version:mdb")) {
        if (value != m_mdbUpdateVersion) { m_mdbUpdateVersion = value; emit mdbUpdateVersionChanged(); }
    } else if (variable == QLatin1String("update-method:mdb")) {
        if (value != m_mdbUpdateMethod) { m_mdbUpdateMethod = value; emit mdbUpdateMethodChanged(); }
    } else if (variable == QLatin1String("error:mdb")) {
        if (value != m_mdbError) { m_mdbError = value; emit mdbErrorChanged(); }
    } else if (variable == QLatin1String("error-message:mdb")) {
        if (value != m_mdbErrorMessage) { m_mdbErrorMessage = value; emit mdbErrorMessageChanged(); }
    } else if (variable == QLatin1String("download-progress:mdb")) {
        int v = value.toInt();
        if (v != m_mdbDownloadProgress) { m_mdbDownloadProgress = v; emit mdbDownloadProgressChanged(); }
    } else if (variable == QLatin1String("install-progress:mdb")) {
        int v = value.toInt();
        if (v != m_mdbInstallProgress) { m_mdbInstallProgress = v; emit mdbInstallProgressChanged(); }
    }

    if (activeChanged) emit isActiveChanged();
}

#include "OtaStore.h"

#include <QStringList>

namespace {
constexpr const char *kErrorStreamKey = "ota:errors";
constexpr int kErrorFetchCount = 200;
}

OtaStore::OtaStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    connect(repo, &MdbRepository::streamFetched,
            this, &OtaStore::onErrorStreamFetched);
}

bool OtaStore::isActive() const
{
    return m_dbcStatus != QLatin1String("idle") || m_mdbStatus != QLatin1String("idle");
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
            {QStringLiteral("dbcErrorEvent"), QStringLiteral("error-event:dbc")},
            {QStringLiteral("dbcDownloadProgress"), QStringLiteral("download-progress:dbc")},
            {QStringLiteral("dbcInstallProgress"), QStringLiteral("install-progress:dbc")},
            {QStringLiteral("mdbStatus"), QStringLiteral("status:mdb")},
            {QStringLiteral("mdbUpdateVersion"), QStringLiteral("update-version:mdb")},
            {QStringLiteral("mdbUpdateMethod"), QStringLiteral("update-method:mdb")},
            {QStringLiteral("mdbError"), QStringLiteral("error:mdb")},
            {QStringLiteral("mdbErrorMessage"), QStringLiteral("error-message:mdb")},
            {QStringLiteral("mdbErrorEvent"), QStringLiteral("error-event:mdb")},
            {QStringLiteral("mdbDownloadProgress"), QStringLiteral("download-progress:mdb")},
            {QStringLiteral("mdbInstallProgress"), QStringLiteral("install-progress:mdb")},
            {QStringLiteral("dbcPreviewChannel"), QStringLiteral("preview-channel:dbc")},
            {QStringLiteral("dbcPreviewStatus"), QStringLiteral("preview-status:dbc")},
            {QStringLiteral("dbcPreviewVersion"), QStringLiteral("preview-version:dbc")},
            {QStringLiteral("dbcPreviewSize"), QStringLiteral("preview-size:dbc")},
            {QStringLiteral("mdbPreviewChannel"), QStringLiteral("preview-channel:mdb")},
            {QStringLiteral("mdbPreviewStatus"), QStringLiteral("preview-status:mdb")},
            {QStringLiteral("mdbPreviewVersion"), QStringLiteral("preview-version:mdb")},
            {QStringLiteral("mdbPreviewSize"), QStringLiteral("preview-size:mdb")},
        },
        {}, {}
    };
}

void OtaStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    bool activeChanged = false;

    if (variable == QLatin1String("status:dbc")) {
        if (value != m_dbcStatus) {
            m_dbcStatus = value;
            emit dbcStatusChanged();
            activeChanged = true;
            if (value == QLatin1String("error")) refreshErrorHistory();
        }
    } else if (variable == QLatin1String("update-version:dbc")) {
        if (value != m_dbcUpdateVersion) { m_dbcUpdateVersion = value; emit dbcUpdateVersionChanged(); }
    } else if (variable == QLatin1String("update-method:dbc")) {
        if (value != m_dbcUpdateMethod) { m_dbcUpdateMethod = value; emit dbcUpdateMethodChanged(); }
    } else if (variable == QLatin1String("error:dbc")) {
        if (value != m_dbcError) { m_dbcError = value; emit dbcErrorChanged(); }
    } else if (variable == QLatin1String("error-message:dbc")) {
        if (value != m_dbcErrorMessage) { m_dbcErrorMessage = value; emit dbcErrorMessageChanged(); }
    } else if (variable == QLatin1String("error-event:dbc")) {
        if (value != m_dbcErrorEvent) { m_dbcErrorEvent = value; refreshErrorHistory(); }
    } else if (variable == QLatin1String("download-progress:dbc")) {
        int v = value.toInt();
        if (v != m_dbcDownloadProgress) { m_dbcDownloadProgress = v; emit dbcDownloadProgressChanged(); }
    } else if (variable == QLatin1String("install-progress:dbc")) {
        int v = value.toInt();
        if (v != m_dbcInstallProgress) { m_dbcInstallProgress = v; emit dbcInstallProgressChanged(); }
    } else if (variable == QLatin1String("status:mdb")) {
        if (value != m_mdbStatus) {
            m_mdbStatus = value;
            emit mdbStatusChanged();
            activeChanged = true;
            if (value == QLatin1String("error")) refreshErrorHistory();
        }
    } else if (variable == QLatin1String("update-version:mdb")) {
        if (value != m_mdbUpdateVersion) { m_mdbUpdateVersion = value; emit mdbUpdateVersionChanged(); }
    } else if (variable == QLatin1String("update-method:mdb")) {
        if (value != m_mdbUpdateMethod) { m_mdbUpdateMethod = value; emit mdbUpdateMethodChanged(); }
    } else if (variable == QLatin1String("error:mdb")) {
        if (value != m_mdbError) { m_mdbError = value; emit mdbErrorChanged(); }
    } else if (variable == QLatin1String("error-message:mdb")) {
        if (value != m_mdbErrorMessage) { m_mdbErrorMessage = value; emit mdbErrorMessageChanged(); }
    } else if (variable == QLatin1String("error-event:mdb")) {
        if (value != m_mdbErrorEvent) { m_mdbErrorEvent = value; refreshErrorHistory(); }
    } else if (variable == QLatin1String("download-progress:mdb")) {
        int v = value.toInt();
        if (v != m_mdbDownloadProgress) { m_mdbDownloadProgress = v; emit mdbDownloadProgressChanged(); }
    } else if (variable == QLatin1String("install-progress:mdb")) {
        int v = value.toInt();
        if (v != m_mdbInstallProgress) { m_mdbInstallProgress = v; emit mdbInstallProgressChanged(); }
    } else if (variable == QLatin1String("preview-channel:dbc")) {
        if (value != m_dbcPreviewChannel) { m_dbcPreviewChannel = value; emit dbcPreviewChanged(); }
    } else if (variable == QLatin1String("preview-status:dbc")) {
        if (value != m_dbcPreviewStatus) { m_dbcPreviewStatus = value; emit dbcPreviewChanged(); }
    } else if (variable == QLatin1String("preview-version:dbc")) {
        if (value != m_dbcPreviewVersion) { m_dbcPreviewVersion = value; emit dbcPreviewChanged(); }
    } else if (variable == QLatin1String("preview-size:dbc")) {
        qint64 v = value.toLongLong();
        if (v != m_dbcPreviewSize) { m_dbcPreviewSize = v; emit dbcPreviewChanged(); }
    } else if (variable == QLatin1String("preview-channel:mdb")) {
        if (value != m_mdbPreviewChannel) { m_mdbPreviewChannel = value; emit mdbPreviewChanged(); }
    } else if (variable == QLatin1String("preview-status:mdb")) {
        if (value != m_mdbPreviewStatus) { m_mdbPreviewStatus = value; emit mdbPreviewChanged(); }
    } else if (variable == QLatin1String("preview-version:mdb")) {
        if (value != m_mdbPreviewVersion) { m_mdbPreviewVersion = value; emit mdbPreviewChanged(); }
    } else if (variable == QLatin1String("preview-size:mdb")) {
        qint64 v = value.toLongLong();
        if (v != m_mdbPreviewSize) { m_mdbPreviewSize = v; emit mdbPreviewChanged(); }
    }

    if (activeChanged) emit isActiveChanged();
}

void OtaStore::refreshErrorHistory()
{
    m_repo->xrevrange(QString::fromLatin1(kErrorStreamKey), kErrorFetchCount);
}

void OtaStore::onErrorStreamFetched(const QString &key, const QVariantList &entries)
{
    if (key != QLatin1String(kErrorStreamKey))
        return;

    QStringList dbcMessages;
    QStringList mdbMessages;
    bool dbcComplete = false;
    bool mdbComplete = false;

    for (const QVariant &value : entries) {
        const QVariantMap fields = value.toMap().value(QStringLiteral("fields")).toMap();
        const QString component = fields.value(QStringLiteral("component")).toString();
        const QString event = fields.value(QStringLiteral("event")).toString();

        QStringList *messages = nullptr;
        bool *complete = nullptr;
        if (component == QLatin1String("dbc")) {
            messages = &dbcMessages;
            complete = &dbcComplete;
        } else if (component == QLatin1String("mdb")) {
            messages = &mdbMessages;
            complete = &mdbComplete;
        } else {
            continue;
        }

        if (*complete)
            continue;
        if (event == QLatin1String("reset")) {
            *complete = true;
        } else if (event == QLatin1String("error")) {
            messages->prepend(fields.value(QStringLiteral("message")).toString());
        }
    }

    const QString dbcHistory = dbcMessages.join(QLatin1Char('\n'));
    if (dbcHistory != m_dbcErrorHistory) {
        m_dbcErrorHistory = dbcHistory;
        emit dbcErrorHistoryChanged();
    }
    const QString mdbHistory = mdbMessages.join(QLatin1Char('\n'));
    if (mdbHistory != m_mdbErrorHistory) {
        m_mdbErrorHistory = mdbHistory;
        emit mdbErrorHistoryChanged();
    }
}

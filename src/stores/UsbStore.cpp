#include "UsbStore.h"

UsbStore::UsbStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    s_instance = this;
}

SyncSettings UsbStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("usb"), 5000,
        {
            {QStringLiteral("status"), QStringLiteral("status")},
            {QStringLiteral("mode"), QStringLiteral("mode")},
            {QStringLiteral("step"), QStringLiteral("step")},
            {QStringLiteral("progress"), QStringLiteral("progress")},
            {QStringLiteral("detail"), QStringLiteral("detail")},
        },
        {}, {}
    };
}

void UsbStore::exitUmsMode()
{
    m_repo->set(QStringLiteral("usb"), QStringLiteral("mode"), QStringLiteral("normal"));
}

void UsbStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("status")) {
        if (value != m_status) { m_status = value; emit statusChanged(); }
    } else if (variable == QLatin1String("mode")) {
        if (value != m_mode) { m_mode = value; emit modeChanged(); }
    } else if (variable == QLatin1String("step")) {
        if (value != m_step) { m_step = value; emit stepChanged(); }
    } else if (variable == QLatin1String("progress")) {
        bool ok = false;
        int pct = value.toInt(&ok);
        if (!ok) pct = 0;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        if (pct != m_progress) { m_progress = pct; emit progressChanged(); }
    } else if (variable == QLatin1String("detail")) {
        if (value != m_detail) { m_detail = value; emit detailChanged(); }
    }
}

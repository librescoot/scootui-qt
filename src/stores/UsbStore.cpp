#include "UsbStore.h"

UsbStore::UsbStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings UsbStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("usb"), 5000,
        {
            {QStringLiteral("status"), QStringLiteral("status")},
            {QStringLiteral("mode"), QStringLiteral("mode")},
        },
        {}, {}
    };
}

void UsbStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("status")) {
        if (value != m_status) { m_status = value; emit statusChanged(); }
    } else if (variable == QLatin1String("mode")) {
        if (value != m_mode) { m_mode = value; emit modeChanged(); }
    }
}

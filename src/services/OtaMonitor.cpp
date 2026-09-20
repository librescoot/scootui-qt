#include "OtaMonitor.h"
#include "ToastService.h"
#include "stores/OtaStore.h"
#include "l10n/Translations.h"

OtaMonitor::OtaMonitor(OtaStore *ota, ToastService *toast, Translations *translations,
                       QObject *parent)
    : QObject(parent)
    , m_ota(ota)
    , m_toast(toast)
    , m_translations(translations)
{
    connect(m_ota, &OtaStore::dbcStatusChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::dbcErrorMessageChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::dbcErrorHistoryChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::dbcUpdateVersionChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::mdbStatusChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::mdbErrorMessageChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::mdbErrorHistoryChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::mdbUpdateVersionChanged, this, &OtaMonitor::evaluate);

    evaluate();
}

QString OtaMonitor::statusFor(Component component) const
{
    return component == Component::Dbc ? m_ota->dbcStatus() : m_ota->mdbStatus();
}

QString OtaMonitor::versionFor(Component component) const
{
    return component == Component::Dbc ? m_ota->dbcUpdateVersion() : m_ota->mdbUpdateVersion();
}

QString OtaMonitor::errorMessageFor(Component component) const
{
    const QString history = component == Component::Dbc
        ? m_ota->dbcErrorHistory() : m_ota->mdbErrorHistory();
    if (!history.isEmpty())
        return history;
    return component == Component::Dbc ? m_ota->dbcErrorMessage() : m_ota->mdbErrorMessage();
}

QString OtaMonitor::componentName(Component component) const
{
    return component == Component::Dbc ? QStringLiteral("DBC") : QStringLiteral("MDB");
}

void OtaMonitor::evaluate()
{
    evaluateComponent(Component::Dbc);
    evaluateComponent(Component::Mdb);
}

void OtaMonitor::evaluateComponent(Component component)
{
    const QString status = statusFor(component);
    const QString version = versionFor(component);
    const QString errorMessage = errorMessageFor(component);
    const bool busy = status == QLatin1String("downloading")
                   || status == QLatin1String("installing");

    QString key = status;
    if (busy)
        key += QLatin1Char(':') + version;
    else if (status == QLatin1String("error"))
        key += QLatin1Char(':') + errorMessage;

    QString &lastKey = component == Component::Dbc ? m_lastDbcKey : m_lastMdbKey;
    if (key == lastKey)
        return;

    // Fields from one Redis update arrive as separate notifications. Wait for
    // the detail needed by the toast instead of announcing an incomplete state.
    if ((busy && version.isEmpty())
        || (status == QLatin1String("error") && errorMessage.isEmpty())) {
        return;
    }

    lastKey = key;

    if (status == QLatin1String("downloading")) {
        m_toast->showInfo(m_translations->otaDownloadingVersionUpdate().arg(version));
    } else if (status == QLatin1String("installing")) {
        m_toast->showInfo(m_translations->otaInstallingVersionUpdate().arg(version));
    } else if (status == QLatin1String("pending-reboot") || status == QLatin1String("rebooting")) {
        m_toast->showInfo(m_translations->otaPendingReboot());
    } else if (status == QLatin1String("error")) {
        const QString detail = componentName(component) + QStringLiteral(": ") + errorMessage;
        m_toast->showError(m_translations->otaUpdateFailedWithMessage().arg(detail));
    }
}

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
    connect(m_ota, &OtaStore::dbcUpdateVersionChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::mdbStatusChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::mdbErrorMessageChanged, this, &OtaMonitor::evaluate);
    connect(m_ota, &OtaStore::mdbUpdateVersionChanged, this, &OtaMonitor::evaluate);

    evaluate();
}

OtaMonitor::Component OtaMonitor::activeComponent() const
{
    const QString dbc = m_ota->dbcStatus();
    if (dbc != QLatin1String("idle") && !dbc.isEmpty())
        return Component::Dbc;
    return Component::Mdb;
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
    return component == Component::Dbc ? m_ota->dbcErrorMessage() : m_ota->mdbErrorMessage();
}

void OtaMonitor::evaluate()
{
    const Component component = activeComponent();
    const QString status = statusFor(component);
    const QString errorMessage = errorMessageFor(component);

    const QString key = QString::number(static_cast<int>(component))
        + QLatin1Char(':') + status + QLatin1Char(':') + errorMessage;
    if (key == m_lastKey)
        return;

    const QString version = versionFor(component);
    const bool busy = status == QLatin1String("downloading")
                   || status == QLatin1String("installing");

    // status and update-version are written together but arrive as separate
    // field updates. Hold the announcement until the version lands so we do not
    // fire a generic "Downloading updates..." followed by the versioned one.
    if (busy && version.isEmpty())
        return;

    // Likewise, status=error and error-message arrive separately; wait for the
    // detail rather than announcing a bare failure first.
    if (status == QLatin1String("error") && errorMessage.isEmpty())
        return;

    m_lastKey = key;

    if (status == QLatin1String("downloading")) {
        m_toast->showInfo(m_translations->otaDownloadingVersionUpdate().arg(version));
    } else if (status == QLatin1String("installing")) {
        m_toast->showInfo(m_translations->otaInstallingVersionUpdate().arg(version));
    } else if (status == QLatin1String("pending-reboot") || status == QLatin1String("rebooting")) {
        m_toast->showInfo(m_translations->otaPendingReboot());
    } else if (status == QLatin1String("error")) {
        m_toast->showError(m_translations->otaUpdateFailedWithMessage().arg(errorMessage));
    }
}

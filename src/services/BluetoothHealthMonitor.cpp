#include "BluetoothHealthMonitor.h"
#include "ToastService.h"
#include "stores/BluetoothStore.h"
#include "l10n/Translations.h"

BluetoothHealthMonitor::BluetoothHealthMonitor(BluetoothStore *bluetooth, ToastService *toast,
                                               Translations *translations, QObject *parent)
    : QObject(parent)
    , m_bluetooth(bluetooth)
    , m_toast(toast)
    , m_translations(translations)
{
    connect(m_bluetooth, &BluetoothStore::serviceHealthChanged, this, &BluetoothHealthMonitor::checkHealth);
    connect(m_bluetooth, &BluetoothStore::serviceErrorChanged, this, &BluetoothHealthMonitor::checkHealth);
    connect(m_bluetooth, &BluetoothStore::faultsChanged, this, &BluetoothHealthMonitor::checkHealth);
}

void BluetoothHealthMonitor::checkHealth()
{
    bool isError = false;
    QString errorMsg;

    if (m_bluetooth->serviceHealth() == QLatin1String("error")) {
        isError = true;
        const QString detail = m_bluetooth->serviceError();
        errorMsg = detail.isEmpty()
            ? m_translations->bluetoothCommError()
            : m_translations->bluetoothError().arg(detail);
    } else if (!m_bluetooth->faults().isEmpty()) {
        // The service raised a fault (serial port / nRF init) and has not
        // cleared it yet.
        isError = true;
        errorMsg = m_translations->bluetoothCommError();
    }

    // Show toast only on false->true transition
    if (isError && !m_wasError) {
        m_toast->showError(errorMsg);
    }
    m_wasError = isError;
}

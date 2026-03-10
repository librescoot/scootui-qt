#include "BluetoothHealthMonitor.h"
#include "ToastService.h"
#include "stores/BluetoothStore.h"

#include <QDateTime>

BluetoothHealthMonitor::BluetoothHealthMonitor(BluetoothStore *bluetooth, ToastService *toast,
                                                 QObject *parent)
    : QObject(parent)
    , m_bluetooth(bluetooth)
    , m_toast(toast)
{
    connect(m_bluetooth, &BluetoothStore::serviceHealthChanged, this, &BluetoothHealthMonitor::checkHealth);
    connect(m_bluetooth, &BluetoothStore::lastUpdateChanged, this, &BluetoothHealthMonitor::checkHealth);
}

void BluetoothHealthMonitor::checkHealth()
{
    bool isError = false;
    QString errorMsg;

    if (m_bluetooth->serviceHealth() == QLatin1String("error")) {
        isError = true;
        errorMsg = m_bluetooth->serviceError().isEmpty()
            ? QStringLiteral("BLE: Communication error")
            : QStringLiteral("BLE: ") + m_bluetooth->serviceError();
    } else if (!m_bluetooth->lastUpdate().isEmpty()) {
        // Check staleness
        QDateTime lastUpdate = QDateTime::fromString(m_bluetooth->lastUpdate(), Qt::ISODate);
        if (lastUpdate.isValid()) {
            qint64 elapsedMs = lastUpdate.msecsTo(QDateTime::currentDateTimeUtc());
            if (elapsedMs > StaleThresholdMs) {
                isError = true;
                errorMsg = QStringLiteral("BLE: Communication timeout");
            }
        }
    }

    // Show toast only on false→true transition
    if (isError && !m_wasError) {
        m_toast->showError(errorMsg);
    }
    m_wasError = isError;
}

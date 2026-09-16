#pragma once

#include <QObject>

class BluetoothStore;
class ToastService;
class Translations;

// Raises a localized error toast when the bluetooth service reports ill health.
//
// Health comes from the FaultSet API the service switched to (ble:fault plus
// service-health/service-error). The old ble[last-update] heartbeat field was
// removed upstream, so there is no timestamp to test for staleness any more.
class BluetoothHealthMonitor : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothHealthMonitor(BluetoothStore *bluetooth, ToastService *toast,
                                     Translations *translations, QObject *parent = nullptr);

private slots:
    void checkHealth();

private:
    BluetoothStore *m_bluetooth;
    ToastService *m_toast;
    Translations *m_translations;
    bool m_wasError = false;
};

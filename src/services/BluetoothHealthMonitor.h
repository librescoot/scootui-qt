#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

class BluetoothStore;
class ToastService;

class BluetoothHealthMonitor : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothHealthMonitor(BluetoothStore *bluetooth, ToastService *toast,
                                     QObject *parent = nullptr);

private slots:
    void checkHealth();

private:
    static constexpr int StaleThresholdMs = 30000;

    BluetoothStore *m_bluetooth;
    ToastService *m_toast;
    bool m_wasError = false;
};

#pragma once

#include <QObject>
#include <QTimer>

class VehicleStore;
class ToastService;
class Translations;

class HandlebarLockMonitor : public QObject
{
    Q_OBJECT

public:
    explicit HandlebarLockMonitor(VehicleStore *vehicle, ToastService *toast,
                                   Translations *translations, QObject *parent = nullptr);

private slots:
    void evaluate();

private:
    static constexpr int DelayMs = 1200;
    static const QString ToastId;

    VehicleStore *m_vehicle;
    ToastService *m_toast;
    Translations *m_translations;
    QTimer *m_delayTimer;
    bool m_showing = false;
};

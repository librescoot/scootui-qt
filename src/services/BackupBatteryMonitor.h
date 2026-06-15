#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class BatteryStore;
class CbBatteryStore;
class AuxBatteryStore;
class ToastService;
class Translations;

// Warns the rider when a backup battery is running low while NO main battery is
// inserted in either slot - i.e. nothing is left to replenish the CBB / AUX and
// the scooter risks losing connectivity (or 12V) if left parked.
//
// This is deliberately distinct from the charging-system warnings in
// BatteryDisplay.qml, which fire only when a main battery IS present but failing
// to charge the backups. These "stranding" warnings fire regardless of seatbox
// state.
class BackupBatteryMonitor : public QObject
{
    Q_OBJECT

public:
    explicit BackupBatteryMonitor(BatteryStore *battery0, BatteryStore *battery1,
                                   CbBatteryStore *cbBattery, AuxBatteryStore *auxBattery,
                                   ToastService *toast, Translations *translations,
                                   QObject *parent = nullptr);

private slots:
    void evaluate();

private:
    bool noMainBattery() const;
    bool cbLowCondition() const;
    bool auxLowCondition() const;

    // CBB reuses the same SoC gate as the charging-system warning.
    static constexpr int CbChargeThreshold = 95;
    // AUX: below 50% SoC, or the rough voltage equivalent as a backstop when SoC
    // is unreported (a healthy 12V pack rests around 12.5V).
    static constexpr int AuxChargeThreshold = 50;
    static constexpr int AuxVoltageThreshold = 12000; // mV
    static constexpr int DebounceMs = 1500;

    static const QString CbToastId;
    static const QString AuxToastId;

    BatteryStore *m_battery0;
    BatteryStore *m_battery1;
    CbBatteryStore *m_cbBattery;
    AuxBatteryStore *m_auxBattery;
    ToastService *m_toast;
    Translations *m_translations;
    QTimer *m_debounceTimer;
    bool m_cbShowing = false;
    bool m_auxShowing = false;
};

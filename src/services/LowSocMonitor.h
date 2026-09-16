#pragma once

#include <QObject>

class BatteryStore;
class ToastService;
class Translations;

// Rider warnings on downward SoC crossings for the main pack (slot 0), ported
// from the Flutter app: empty at 0%, max-speed-cut below 5%, power reduced at
// <=10% and <20%. Only downward crossings announce, and the first reading after
// (re)insertion is the baseline, so booting on a low pack does not nag.
class LowSocMonitor : public QObject
{
    Q_OBJECT

public:
    explicit LowSocMonitor(BatteryStore *battery0, ToastService *toast,
                           Translations *translations, QObject *parent = nullptr);

private slots:
    void checkSoc();
    void reseed();

private:
    BatteryStore *m_battery0;
    ToastService *m_toast;
    Translations *m_translations;
    int m_lastSoc = 0;
    bool m_hasLastSoc = false;
};

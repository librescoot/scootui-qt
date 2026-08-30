#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class BatteryStore;
class CbBatteryStore;
class AuxBatteryStore;
class VehicleStore;

// Drives the status bar's battery warning icons: evaluates the conditions in
// BatteryAlertPolicy against the live stores and debounces each icon so a
// transient reading (a pack settling after a load step) doesn't flash a
// warning. Also exposes the range/value formatting so the number the bar
// shows comes from the same model everywhere.
class BatteryAlertModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showCbWarning READ showCbWarning NOTIFY alertsChanged)
    Q_PROPERTY(bool showAuxWarning READ showAuxWarning NOTIFY alertsChanged)
    Q_PROPERTY(bool showCbStranded READ showCbStranded NOTIFY alertsChanged)
    Q_PROPERTY(bool showAuxStranded READ showAuxStranded NOTIFY alertsChanged)
    // Undebounced "low" reads for the optional level-glyph visibility modes.
    Q_PROPERTY(bool cbLow READ cbLow NOTIFY alertsChanged)
    Q_PROPERTY(bool auxLow READ auxLow NOTIFY alertsChanged)

public:
    explicit BatteryAlertModel(BatteryStore *battery0, BatteryStore *battery1,
                               CbBatteryStore *cbBattery, AuxBatteryStore *auxBattery,
                               VehicleStore *vehicle, QObject *parent = nullptr);

    bool showCbWarning() const { return m_cbWarning.shown; }
    bool showAuxWarning() const { return m_auxWarning.shown; }
    bool showCbStranded() const { return m_cbStranded.shown; }
    bool showAuxStranded() const { return m_auxStranded.shown; }
    bool cbLow() const;
    bool auxLow() const;

    Q_INVOKABLE QString valueText(int charge, double soh, bool asRange,
                                  bool withDecimals) const;

signals:
    void alertsChanged();

private:
    struct Debounce {
        QTimer timer;
        bool active = false;
        bool shown = false;
    };

    void reevaluate();
    // Rising edge arms the timer; the timer showing the icon re-checks the
    // condition; a falling edge clears icon and timer immediately.
    bool updateDebounce(Debounce &d, bool condition);

    BatteryStore *m_battery0;
    BatteryStore *m_battery1;
    CbBatteryStore *m_cbBattery;
    AuxBatteryStore *m_auxBattery;
    VehicleStore *m_vehicle;

    Debounce m_cbWarning;
    Debounce m_auxWarning;
    Debounce m_cbStranded;
    Debounce m_auxStranded;
};

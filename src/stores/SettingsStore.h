#pragma once

#include "SyncableStore.h"
#include "models/Enums.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class SettingsStore : public SyncableStore
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool showRawSpeed READ showRawSpeed NOTIFY showRawSpeedChanged)
    Q_PROPERTY(QString batteryDisplayMode READ batteryDisplayMode NOTIFY batteryDisplayModeChanged)
    Q_PROPERTY(int mapType READ mapType NOTIFY mapTypeChanged)
    Q_PROPERTY(int mapRenderMode READ mapRenderMode NOTIFY mapRenderModeChanged)
    Q_PROPERTY(QString valhallaUrl READ valhallaUrl NOTIFY valhallaUrlChanged)
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(int powerDisplayMode READ powerDisplayMode NOTIFY powerDisplayModeChanged)
    Q_PROPERTY(QString blinkerStyle READ blinkerStyle NOTIFY blinkerStyleChanged)
    Q_PROPERTY(bool dualBattery READ dualBattery NOTIFY dualBatteryChanged)
    Q_PROPERTY(QString showGps READ showGps NOTIFY showGpsChanged)
    Q_PROPERTY(QString showBluetooth READ showBluetooth NOTIFY showBluetoothChanged)
    Q_PROPERTY(QString showCloud READ showCloud NOTIFY showCloudChanged)
    Q_PROPERTY(QString showInternet READ showInternet NOTIFY showInternetChanged)
    Q_PROPERTY(QString showClock READ showClock NOTIFY showClockChanged)
    Q_PROPERTY(bool alarmEnabled READ alarmEnabled NOTIFY alarmEnabledChanged)
    Q_PROPERTY(bool alarmHonk READ alarmHonk NOTIFY alarmHonkChanged)
    Q_PROPERTY(QString alarmDuration READ alarmDuration NOTIFY alarmDurationChanged)

public:
    explicit SettingsStore(MdbRepository *repo, QObject *parent = nullptr);
    static SettingsStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    QString theme() const { return m_theme; }
    QString mode() const { return m_mode; }
    bool showRawSpeed() const { return m_showRawSpeed == QLatin1String("true"); }
    QString batteryDisplayMode() const { return m_batteryDisplayMode; }
    int mapType() const { return static_cast<int>(m_mapType); }
    int mapRenderMode() const { return static_cast<int>(m_mapRenderMode); }
    QString valhallaUrl() const { return m_valhallaUrl; }
    QString language() const { return m_language; }
    int powerDisplayMode() const { return static_cast<int>(m_powerDisplayMode); }
    QString blinkerStyle() const { return m_blinkerStyle; }
    bool dualBattery() const { return m_dualBattery == QLatin1String("true"); }
    QString showGps() const { return m_showGps; }
    QString showBluetooth() const { return m_showBluetooth; }
    QString showCloud() const { return m_showCloud; }
    QString showInternet() const { return m_showInternet; }
    QString showClock() const { return m_showClock; }
    bool alarmEnabled() const { return m_alarmEnabled == QLatin1String("true"); }
    bool alarmHonk() const { return m_alarmHonk == QLatin1String("true"); }
    QString alarmDuration() const { return m_alarmDuration; }

    // Helper
    bool showBatteryAsRange() const { return m_batteryDisplayMode == QLatin1String("range"); }
    bool blinkerOverlayEnabled() const { return m_blinkerStyle == QLatin1String("overlay"); }

signals:
    void themeChanged();
    void modeChanged();
    void showRawSpeedChanged();
    void batteryDisplayModeChanged();
    void mapTypeChanged();
    void mapRenderModeChanged();
    void valhallaUrlChanged();
    void languageChanged();
    void powerDisplayModeChanged();
    void blinkerStyleChanged();
    void dualBatteryChanged();
    void showGpsChanged();
    void showBluetoothChanged();
    void showCloudChanged();
    void showInternetChanged();
    void showClockChanged();
    void alarmEnabledChanged();
    void alarmHonkChanged();
    void alarmDurationChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_theme = QStringLiteral("dark");
    QString m_mode = QStringLiteral("speedometer");
    QString m_showRawSpeed = QStringLiteral("false");
    QString m_batteryDisplayMode = QStringLiteral("percentage");
    ScootEnums::MapType m_mapType = ScootEnums::MapType::Offline;
    ScootEnums::MapRenderMode m_mapRenderMode = ScootEnums::MapRenderMode::Vector;
    QString m_valhallaUrl;
    QString m_language = QStringLiteral("en");
    ScootEnums::PowerDisplayMode m_powerDisplayMode = ScootEnums::PowerDisplayMode::Kw;
    QString m_blinkerStyle = QStringLiteral("icon");
    QString m_dualBattery = QStringLiteral("true");
    QString m_showGps = QStringLiteral("error");
    QString m_showBluetooth = QStringLiteral("active-or-error");
    QString m_showCloud = QStringLiteral("never");
    QString m_showInternet = QStringLiteral("never");
    QString m_showClock = QStringLiteral("always");
    QString m_alarmEnabled = QStringLiteral("false");
    QString m_alarmHonk = QStringLiteral("false");
    QString m_alarmDuration;
    static inline SettingsStore *s_instance = nullptr;
};

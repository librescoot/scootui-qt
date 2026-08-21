#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlengine.h>

class MdbRepository;

class SettingsService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit SettingsService(MdbRepository *repo, QObject *parent = nullptr);

    Q_INVOKABLE void updateMode(const QString &mode);
    Q_INVOKABLE void updateTheme(const QString &theme);
    Q_INVOKABLE void updateAutoTheme(bool enabled);
    Q_INVOKABLE void updateBacklightMode(const QString &mode);
    Q_INVOKABLE void updateLanguage(const QString &lang);
    Q_INVOKABLE void updateBatteryDisplayMode(const QString &mode);
    Q_INVOKABLE void updateBlinkerStyle(const QString &style);
    Q_INVOKABLE void updateDbcBlinkerLed(bool enabled);
    Q_INVOKABLE void updateDualBattery(bool enabled);
    Q_INVOKABLE void updateHornWhenSeatboxOpen(bool enabled);
    Q_INVOKABLE void updateShowGps(const QString &mode);
    Q_INVOKABLE void updateShowBluetooth(const QString &mode);
    Q_INVOKABLE void updateShowCloud(const QString &mode);
    Q_INVOKABLE void updateShowInternet(const QString &mode);
    Q_INVOKABLE void updateShowClock(const QString &mode);
    Q_INVOKABLE void updateShowTemperature(const QString &mode);
    Q_INVOKABLE void updateShowCbBattery(const QString &mode);
    Q_INVOKABLE void updateShowAuxBattery(const QString &mode);
    Q_INVOKABLE void updateAlarmEnabled(bool enabled);
    Q_INVOKABLE void updateAlarmHonk(bool enabled);
    Q_INVOKABLE void updateAlarmDuration(int seconds);
    Q_INVOKABLE void updateMapType(const QString &type);
    Q_INVOKABLE void updateMapViewMode(const QString &mode);
    Q_INVOKABLE void updateMapNorthOriented(bool enabled);
    Q_INVOKABLE void updateMapRenderMode(const QString &mode);
    Q_INVOKABLE void updateValhallaEndpoint(const QString &url);
    Q_INVOKABLE void updateRoutePreference(const QString &pref);
    Q_INVOKABLE void updateAvoidCobblestone(const QString &level);
    Q_INVOKABLE void updatePowerDisplayMode(const QString &mode);
    Q_INVOKABLE void updateHopOnCombo(const QString &combo);
    Q_INVOKABLE void updateMapCheckForUpdates(bool enabled);
    Q_INVOKABLE void updateMapAutoDownload(bool enabled);
    Q_INVOKABLE void updateMilestoneCelebrations(bool enabled);
    Q_INVOKABLE QString toggleBootAnimation();

    // OTA settings apply to the whole scooter, so each of these writes the
    // MDB and the DBC key together. Splitting them would let the two boards
    // drift onto different channels, which the release index has no story for.
    Q_INVOKABLE void updateOtaChannel(const QString &channel);
    Q_INVOKABLE void updateOtaMethod(const QString &method);
    Q_INVOKABLE void updateOtaCheckInterval(const QString &interval);
    // Queues check-now for both components. Each update-service instance
    // listens on its own scooter:update:{component} queue; there is no
    // broadcast queue that reaches both.
    Q_INVOKABLE void triggerUpdateCheck();
    // Asks both components what a switch to channel would fetch. Answers land
    // in the ota hash's preview-* fields, mirrored by OtaStore.
    Q_INVOKABLE void requestChannelPreview(const QString &channel);

private:
    void writeSetting(const QString &key, const QString &value);
    void writeOtaSetting(const QString &suffix, const QString &value);
    MdbRepository *m_repo;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static SettingsService *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(SettingsService *instance) { s_qmlInstance = instance; }

private:
    static inline SettingsService *s_qmlInstance = nullptr;
};

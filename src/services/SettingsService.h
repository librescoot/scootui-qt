#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class MdbRepository;

class QQmlEngine;
class QJSEngine;

class SettingsService : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT

public:
    explicit SettingsService(MdbRepository *repo, QObject *parent = nullptr);
    static SettingsService *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    Q_INVOKABLE void updateMode(const QString &mode);
    Q_INVOKABLE void updateTheme(const QString &theme);
    Q_INVOKABLE void updateAutoTheme(bool enabled);
    Q_INVOKABLE void updateLanguage(const QString &lang);
    Q_INVOKABLE void updateBatteryDisplayMode(const QString &mode);
    Q_INVOKABLE void updateBlinkerStyle(const QString &style);
    Q_INVOKABLE void updateDualBattery(bool enabled);
    Q_INVOKABLE void updateShowGps(const QString &mode);
    Q_INVOKABLE void updateShowBluetooth(const QString &mode);
    Q_INVOKABLE void updateShowCloud(const QString &mode);
    Q_INVOKABLE void updateShowInternet(const QString &mode);
    Q_INVOKABLE void updateShowClock(const QString &mode);
    Q_INVOKABLE void updateAlarmEnabled(bool enabled);
    Q_INVOKABLE void updateAlarmHonk(bool enabled);
    Q_INVOKABLE void updateAlarmDuration(int seconds);
    Q_INVOKABLE void updateMapType(const QString &type);
    Q_INVOKABLE void updateMapRenderMode(const QString &mode);
    Q_INVOKABLE void updateValhallaEndpoint(const QString &url);
    Q_INVOKABLE void updatePowerDisplayMode(const QString &mode);
    Q_INVOKABLE QString toggleBootAnimation();

private:
    void writeSetting(const QString &key, const QString &value);
    MdbRepository *m_repo;
    static inline SettingsService *s_instance = nullptr;
};

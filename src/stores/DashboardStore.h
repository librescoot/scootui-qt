#pragma once

#include "SyncableStore.h"

class DashboardStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QString debugMode READ debugMode NOTIFY debugModeChanged)
    // Ambient illuminance in lux and the resulting backlight level, both from
    // the `dashboard` hash (dbc-backlight-service). <0 means not yet reported.
    Q_PROPERTY(double brightness READ brightness NOTIFY brightnessChanged)
    Q_PROPERTY(int backlight READ backlight NOTIFY backlightChanged)
    // The override, not the level: false means dbc-backlight is holding the
    // panel at 0 and nothing on screen can be seen.
    Q_PROPERTY(bool backlightEnabled READ backlightEnabled NOTIFY backlightEnabledChanged)

public:
    explicit DashboardStore(MdbRepository *repo, QObject *parent = nullptr);

    QString debugMode() const { return m_debugMode; }
    double brightness() const { return m_brightness; }
    int backlight() const { return m_backlight; }
    bool backlightEnabled() const { return m_backlightEnabled; }

    Q_INVOKABLE void setBacklightEnabled(bool enabled);

signals:
    void debugModeChanged();
    void brightnessChanged();
    void backlightChanged();
    void backlightEnabledChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_debugMode = QStringLiteral("off");
    double m_brightness = -1.0;
    int m_backlight = -1;
    // Absent means enabled, matching dbc-backlight's own default.
    bool m_backlightEnabled = true;
};

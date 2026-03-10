#include "SettingsService.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>

SettingsService::SettingsService(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
}

void SettingsService::writeSetting(const QString &key, const QString &value)
{
    m_repo->set(QStringLiteral("settings"), key, value);
}

void SettingsService::updateTheme(const QString &theme)
{
    writeSetting(QStringLiteral("dashboard.theme"), theme);
}

void SettingsService::updateAutoTheme(bool enabled)
{
    writeSetting(QStringLiteral("dashboard.theme"), enabled ? QStringLiteral("auto") : QStringLiteral("dark"));
}

void SettingsService::updateLanguage(const QString &lang)
{
    writeSetting(QStringLiteral("dashboard.language"), lang);
}

void SettingsService::updateBatteryDisplayMode(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.battery-display-mode"), mode);
}

void SettingsService::updateBlinkerStyle(const QString &style)
{
    writeSetting(QStringLiteral("dashboard.blinker-style"), style);
}

void SettingsService::updateDualBattery(bool enabled)
{
    writeSetting(QStringLiteral("scooter.dual-battery"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void SettingsService::updateShowGps(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.show-gps"), mode);
}

void SettingsService::updateShowBluetooth(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.show-bluetooth"), mode);
}

void SettingsService::updateShowCloud(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.show-cloud"), mode);
}

void SettingsService::updateShowInternet(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.show-internet"), mode);
}

void SettingsService::updateShowClock(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.show-clock"), mode);
}

void SettingsService::updateAlarmEnabled(bool enabled)
{
    writeSetting(QStringLiteral("alarm.enabled"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void SettingsService::updateAlarmHonk(bool enabled)
{
    writeSetting(QStringLiteral("alarm.honk"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void SettingsService::updateAlarmDuration(int seconds)
{
    writeSetting(QStringLiteral("alarm.duration"), QString::number(seconds));
}

void SettingsService::updateMapType(const QString &type)
{
    writeSetting(QStringLiteral("dashboard.map.type"), type);
}

void SettingsService::updateMapRenderMode(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.map.render-mode"), mode);
}

void SettingsService::updateValhallaEndpoint(const QString &url)
{
    writeSetting(QStringLiteral("dashboard.valhalla-url"), url);
}

void SettingsService::updatePowerDisplayMode(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.power-display-mode"), mode);
}

void SettingsService::togglePlymouthTheme()
{
#ifdef Q_OS_LINUX
    // Read current theme from plymouth config
    QString currentTheme = QStringLiteral("librescoot");
    QFile confFile(QStringLiteral("/etc/plymouth/plymouthd.conf"));
    if (confFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&confFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith(QLatin1String("Theme="))) {
                currentTheme = line.mid(6);
                break;
            }
        }
        confFile.close();
    }

    // Toggle between librescoot and windowsxp
    QString newTheme = (currentTheme == QLatin1String("windowsxp"))
        ? QStringLiteral("librescoot") : QStringLiteral("windowsxp");

    // Write updated plymouth config
    if (confFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&confFile);
        out << "[Daemon]\n";
        out << "Theme=" << newTheme << "\n";
        confFile.close();
    }

    // Write marker file for boot-animation service
    QFile markerFile(QStringLiteral("/data/plymouth-theme"));
    if (markerFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&markerFile);
        out << newTheme;
        markerFile.close();
    }

    qDebug() << "Plymouth theme toggled to:" << newTheme;
#endif
}

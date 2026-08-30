#include "SettingsService.h"
#include "repositories/MdbRepository.h"
#include "stores/SettingsStore.h"
#include "core/AppConfig.h"

#include <QDebug>
#include <QProcess>
#include "repositories/RedisSchema.h"

SettingsService::SettingsService(MdbRepository *repo, SettingsStore *settings,
                                 QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_settings(settings)
{
#ifdef Q_OS_LINUX
    // Prefetch the boot_animation theme so toggleBootAnimation never has to
    // read the U-Boot env synchronously.
    auto *readProc = new QProcess(this);
    connect(readProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, readProc](int, QProcess::ExitStatus) {
        // A toggle that raced the prefetch already knows the newer value.
        if (m_bootAnimationTheme.isEmpty()) {
            const QString current
                = QString::fromUtf8(readProc->readAllStandardOutput()).trimmed();
            m_bootAnimationTheme
                = current.isEmpty() ? QStringLiteral("librescoot") : current;
        }
        readProc->deleteLater();
    });
    readProc->start(QStringLiteral("fw_printenv"),
                    {QStringLiteral("-n"), QStringLiteral("boot_animation")});
#endif
}

void SettingsService::writeSetting(const QString &key, const QString &value)
{
    m_repo->set(RedisSchema::hash::Settings, key, value);
}

void SettingsService::writeOtaSetting(const QString &suffix, const QString &value)
{
    writeSetting(QStringLiteral("updates.mdb.") + suffix, value);
    writeSetting(QStringLiteral("updates.dbc.") + suffix, value);
}

void SettingsService::updateOtaChannel(const QString &channel)
{
    writeOtaSetting(QStringLiteral("channel"), channel);
}

void SettingsService::updateOtaMethod(const QString &method)
{
    writeOtaSetting(QStringLiteral("method"), method);
}

void SettingsService::updateOtaCheckInterval(const QString &interval)
{
    writeOtaSetting(QStringLiteral("check-interval"), interval);
}

void SettingsService::triggerUpdateCheck()
{
    m_repo->push(RedisSchema::list::ScooterUpdateMdb, QStringLiteral("check-now"));
    m_repo->push(RedisSchema::list::ScooterUpdateDbc, QStringLiteral("check-now"));
}

void SettingsService::disableServiceMode()
{
    m_repo->push(RedisSchema::list::SettingsOverlay, QStringLiteral("clear:service"));
}

void SettingsService::requestChannelPreview(const QString &channel)
{
    const QString command = QStringLiteral("preview-channel:") + channel;
    m_repo->push(RedisSchema::list::ScooterUpdateMdb, command);
    m_repo->push(RedisSchema::list::ScooterUpdateDbc, command);
}

void SettingsService::updateMode(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.mode"), mode);
    // dashboard.mode is the one setting the dashboard keeps a second copy of:
    // Navigator switches the screen the moment this is called, without
    // waiting for the read-back. The store has to move with it, or the two
    // disagree and the disagreement is invisible.
    //
    // settings-service owns the key while the service overlay is up and
    // re-asserts its value in the same callback that sees an edit to it. With
    // the store left on the old value, that re-assert reads back as the value
    // the store already holds, no signal is emitted, and the screen sits
    // wherever it moved to with dashboard.mode saying something else. That is
    // how leaving the debug screen in service mode stranded the dashboard on
    // the cluster with no way back to the debug screen.
    if (m_settings)
        m_settings->applyLocalWrite(QStringLiteral("dashboard.mode"), mode);
}

void SettingsService::updateTheme(const QString &theme)
{
    writeSetting(QStringLiteral("dashboard.theme"), theme);
}

void SettingsService::updateAutoTheme(bool enabled)
{
    writeSetting(QStringLiteral("dashboard.theme"), enabled ? QStringLiteral("auto") : QStringLiteral("dark"));
}

void SettingsService::updateBacklightMode(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.backlight-mode"), mode);
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

void SettingsService::updateDbcBlinkerLed(bool enabled)
{
    writeSetting(QStringLiteral("scooter.dbc-blinker-led"),
                 enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"));
}

void SettingsService::updateDualBattery(bool enabled)
{
    writeSetting(QStringLiteral("scooter.dual-battery"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void SettingsService::updateHornWhenSeatboxOpen(bool enabled)
{
    writeSetting(QStringLiteral("scooter.horn-when-seatbox-open"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
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

void SettingsService::updateShowTemperature(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.show-temperature"), mode);
}

void SettingsService::updateShowCbBattery(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.show-cb-battery"), mode);
}

void SettingsService::updateShowAuxBattery(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.show-aux-battery"), mode);
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

void SettingsService::updateMapViewMode(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.map.view-mode"), mode);
}

void SettingsService::updateMapNorthOriented(bool enabled)
{
    writeSetting(QStringLiteral("dashboard.map.north-oriented"),
                 enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void SettingsService::updateMapRenderMode(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.map.render-mode"), mode);
}

void SettingsService::updateValhallaEndpoint(const QString &url)
{
    writeSetting(QStringLiteral("dashboard.valhalla-url"), url);
}

void SettingsService::updateRoutePreference(const QString &pref)
{
    writeSetting(QStringLiteral("dashboard.route-preference"), pref);
}

void SettingsService::updateAvoidCobblestone(const QString &level)
{
    writeSetting(QStringLiteral("dashboard.avoid-cobblestone"), level);
}

void SettingsService::updatePowerDisplayMode(const QString &mode)
{
    writeSetting(QStringLiteral("dashboard.power-display-mode"), mode);
}

void SettingsService::updateHopOnCombo(const QString &combo)
{
    writeSetting(QStringLiteral("dashboard.hop-on-combo"), combo);
}

void SettingsService::updateMapCheckForUpdates(bool enabled)
{
    writeSetting(QStringLiteral("dashboard.maps.check-for-updates"),
                 enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void SettingsService::updateMapAutoDownload(bool enabled)
{
    writeSetting(QStringLiteral("dashboard.maps.auto-download"),
                 enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void SettingsService::updateMilestoneCelebrations(bool enabled)
{
    writeSetting(QStringLiteral("dashboard.milestone-celebrations"),
                 enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

QString SettingsService::toggleBootAnimation()
{
#ifdef Q_OS_LINUX
    const QString current = m_bootAnimationTheme.isEmpty()
        ? QStringLiteral("librescoot") : m_bootAnimationTheme;
    const QString next = (current == QLatin1String("windowsxp"))
        ? QStringLiteral("librescoot") : QStringLiteral("windowsxp");
    m_bootAnimationTheme = next;

    auto *writeProc = new QProcess(this);
    connect(writeProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [writeProc, next](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0)
            qWarning() << "fw_setenv boot_animation failed:"
                       << writeProc->readAllStandardError();
        else
            qDebug() << "boot_animation toggled to:" << next;
        writeProc->deleteLater();
    });
    writeProc->start(QStringLiteral("fw_setenv"),
                     {QStringLiteral("boot_animation"), next});

    return next;
#else
    return {};
#endif
}

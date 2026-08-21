#include "AutoThemeService.h"
#include "core/AppConfig.h"
#include "repositories/MdbRepository.h"
#include "stores/ThemeStore.h"
#include <QDebug>

AutoThemeService::AutoThemeService(MdbRepository *repo, ThemeStore *themeStore,
                                   QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_themeStore(themeStore)
    , m_pollTimer(new QTimer(this))
    , m_lockoutTimer(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &AutoThemeService::checkBrightness);

    m_lockoutTimer->setSingleShot(true);

    // Also listen for pubsub brightness updates
    m_repo->subscribe(QStringLiteral("dashboard"), [this](const QString &, const QString &msg) {
        if (msg.contains(QLatin1String(AppConfig::brightnessKey)) && m_enabled) {
            QMetaObject::invokeMethod(this, &AutoThemeService::checkBrightness,
                                      Qt::QueuedConnection);
        }
    });
}

AutoThemeService::~AutoThemeService()
{
    m_pollTimer->stop();
    m_lockoutTimer->stop();
}

void AutoThemeService::setEnabled(bool enabled)
{
    bool wasEnabled = m_enabled;
    m_enabled = enabled;
    if (enabled) {
        if (!wasEnabled)
            m_forceSync = true;
        m_pollTimer->start(1000);
        checkBrightness();
    } else {
        m_pollTimer->stop();
        m_lockoutTimer->stop();
        m_pendingSince.invalidate();
    }
}

void AutoThemeService::checkBrightness()
{
    if (!m_enabled) return;

    const QString val = m_repo->get(QStringLiteral("dashboard"),
                                    QLatin1String(AppConfig::brightnessKey));
    if (val.isEmpty()) return;

    bool ok = false;
    double lux = val.toDouble(&ok);
    if (!ok) return;

    processBrightness(lux);
}

void AutoThemeService::processBrightness(double lux)
{
    // The threshold test runs on the raw reading. An EMA in front of it only
    // delays the decision without rejecting anything a dwell doesn't reject
    // better: from daylight, the alpha 0.7 filter this used to carry needed
    // seven consecutive samples to decay below the 8 lux line, so riding into
    // a tunnel left a white screen up for seven seconds.
    bool wantDark = m_currentlyDark;
    if (m_currentlyDark) {
        if (lux > AppConfig::autoThemeLightThreshold)
            wantDark = false;
    } else {
        if (lux < AppConfig::autoThemeDarkThreshold)
            wantDark = true;
    }

    if (m_forceSync) {
        // Re-entering auto mode: resync immediately, ignoring dwell and lockout.
        m_forceSync = false;
        commitFlip(wantDark);
        return;
    }

    if (wantDark == m_currentlyDark) {
        m_pendingSince.invalidate();
        return;
    }

    // The reading has to stay past the threshold for the whole dwell, and a
    // single contrary one starts it over. Duration is the only thing that
    // separates a tunnel from a tree-lined street: both are a deep drop from a
    // lit baseline, but the shadows come back within a second or two at riding
    // speed while the tunnel holds. dbc-backlight samples at about 5.8 Hz, so
    // the dwell covers roughly fourteen readings and a tunnel commits a little
    // under three seconds after it goes dark.
    if (!m_pendingSince.isValid()) {
        m_pendingSince.start();
        return;
    }
    if (m_pendingSince.elapsed() < AppConfig::autoThemeDwellMs)
        return;

    // Dwell is satisfied but a recent flip still holds the floor. Keep the
    // pending timer running so the flip lands as soon as the lockout expires.
    if (m_lockoutTimer->isActive())
        return;

    commitFlip(wantDark);
}

void AutoThemeService::commitFlip(bool dark)
{
    m_pendingSince.invalidate();
    m_currentlyDark = dark;
    m_themeStore->setTheme(dark ? QStringLiteral("dark") : QStringLiteral("light"));
    m_lockoutTimer->start(AppConfig::autoThemeLockoutMs);
}

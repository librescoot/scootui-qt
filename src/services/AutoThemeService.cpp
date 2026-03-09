#include "AutoThemeService.h"
#include "repositories/MdbRepository.h"
#include "stores/ThemeStore.h"
#include <QDebug>

AutoThemeService::AutoThemeService(MdbRepository *repo, ThemeStore *themeStore,
                                   QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_themeStore(themeStore)
    , m_pollTimer(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &AutoThemeService::checkBrightness);

    // Also listen for pubsub brightness updates
    m_repo->subscribe(QStringLiteral("dashboard"), [this](const QString &, const QString &msg) {
        if (msg.contains(QLatin1String("brightness")) && m_enabled) {
            QMetaObject::invokeMethod(this, &AutoThemeService::checkBrightness,
                                      Qt::QueuedConnection);
        }
    });
}

AutoThemeService::~AutoThemeService()
{
    m_pollTimer->stop();
}

void AutoThemeService::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (enabled) {
        m_pollTimer->start(1000);
        checkBrightness();
    } else {
        m_pollTimer->stop();
    }
}

void AutoThemeService::checkBrightness()
{
    if (!m_enabled) return;

    const QString val = m_repo->get(QStringLiteral("dashboard"), QStringLiteral("brightness"));
    if (val.isEmpty()) return;

    bool ok = false;
    double lux = val.toDouble(&ok);
    if (!ok) return;

    processBrightness(lux);
}

void AutoThemeService::processBrightness(double rawLux)
{
    if (m_smoothedBrightness < 0) {
        m_smoothedBrightness = rawLux;
    } else {
        m_smoothedBrightness = SMOOTHING_ALPHA * rawLux
                             + (1.0 - SMOOTHING_ALPHA) * m_smoothedBrightness;
    }

    bool shouldBeDark = m_currentlyDark;

    if (m_currentlyDark) {
        if (m_smoothedBrightness > LIGHT_THRESHOLD)
            shouldBeDark = false;
    } else {
        if (m_smoothedBrightness < DARK_THRESHOLD)
            shouldBeDark = true;
    }

    if (shouldBeDark != m_currentlyDark) {
        m_currentlyDark = shouldBeDark;
        m_themeStore->setTheme(shouldBeDark ? QStringLiteral("dark") : QStringLiteral("light"));
    }
}

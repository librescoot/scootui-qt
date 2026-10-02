#include "AutoThemeService.h"
#include "core/AppConfig.h"
#include "repositories/MdbRepository.h"
#include "stores/ThemeStore.h"
#include <QDebug>

#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

AmbientLightProbe::AmbientLightProbe(QString path, Reader reader)
    : m_path(path.isEmpty() ? defaultPath() : std::move(path))
    , m_reader(reader ? std::move(reader) : [](const QString &path) {
        const QByteArray encoded = path.toUtf8();
        const int fd = ::open(encoded.constData(), O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            return QString();
        char buffer[128];
        const ssize_t length = ::read(fd, buffer, sizeof(buffer));
        ::close(fd);
        if (length <= 0)
            return QString();
        return QString::fromUtf8(buffer, int(length)).trimmed();
    })
{
}

AmbientLightProbe::~AmbientLightProbe()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
        m_condition.notify_one();
    }
    if (m_thread.joinable())
        m_thread.join();
}

void AmbientLightProbe::start()
{
    bool expected = false;
    if (!m_started.compare_exchange_strong(expected, true))
        return;
    requestSample();
    m_thread = std::thread([this] { run(); });
}

quint64 AmbientLightProbe::requestSample()
{
    if (!m_started.load()) {
        start();
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_requestedGeneration;
    }
    quint64 generation;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        generation = ++m_requestedGeneration;
        m_requested = true;
        m_finished.store(false);
    }
    m_condition.notify_one();
    return generation;
}

QString AmbientLightProbe::sample() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_result;
}

QString AmbientLightProbe::defaultPath()
{
    const QString configured = qEnvironmentVariable("SCOOTUI_AMBIENT_LIGHT_PATH");
    return configured.isEmpty()
        ? QStringLiteral("/sys/bus/iio/devices/iio:device0/in_illuminance_input")
        : configured;
}

void AmbientLightProbe::run()
{
    for (;;) {
        quint64 generation;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this] { return m_stopping || m_requested; });
            if (m_stopping)
                return;
            generation = m_requestedGeneration;
            m_requested = false;
            m_finished.store(false);
        }

        QString value;
        try {
            value = m_reader(m_path);
        } catch (...) {
            value.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_result = value;
            m_generation.store(generation);
            m_finished.store(true);
        }
    }
}

AutoThemeService::AutoThemeService(MdbRepository *repo, ThemeStore *themeStore,
                                   QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_themeStore(themeStore)
    , m_pollTimer(new QTimer(this))
    , m_lockoutTimer(new QTimer(this))
    , m_localProbeTimer(new QTimer(this))
    , m_bootSampleEligible(themeStore->isAutoMode())
{
    connect(m_pollTimer, &QTimer::timeout, this, &AutoThemeService::checkBrightness);
    connect(m_localProbeTimer, &QTimer::timeout, this, &AutoThemeService::checkLocalSensor);
    m_localProbeTimer->setInterval(25);

    m_lockoutTimer->setSingleShot(true);

    connect(m_repo, &MdbRepository::fieldFetched, this,
            [this](const QString &channel, const QString &field, const QString &) {
        if (channel != QLatin1String("dashboard")
            || field != QLatin1String(AppConfig::brightnessKey))
            return;
        if (!m_enabled)
            return;
        m_redisFresh = true;
        checkBrightness();
    });

    // Fetch the value named by the notification before treating it as fresh.
    m_dashboardSubscriptionId = m_repo->subscribe(
        QStringLiteral("dashboard"), [this](const QString &, const QString &msg) {
            if (msg.contains(QLatin1String(AppConfig::brightnessKey))) {
                QMetaObject::invokeMethod(this, [this]() {
                    if (m_enabled)
                        m_repo->requestField(QStringLiteral("dashboard"),
                                             QLatin1String(AppConfig::brightnessKey));
                }, Qt::QueuedConnection);
            }
        });
}

AutoThemeService::~AutoThemeService()
{
    m_pollTimer->stop();
    m_localProbeTimer->stop();
    m_lockoutTimer->stop();
    if (m_dashboardSubscriptionId != 0)
        m_repo->unsubscribe(QStringLiteral("dashboard"), m_dashboardSubscriptionId);
}

void AutoThemeService::setLocalProbe(AmbientLightProbe *probe)
{
    m_localProbe = probe;
    if (m_localProbe)
        m_localProbe->start();
}

void AutoThemeService::setEnabled(bool enabled)
{
    if (enabled && m_themeStore && !m_themeStore->isAutoMode())
        enabled = false;
    bool wasEnabled = m_enabled;
    m_enabled = enabled;
    if (enabled) {
        if (!wasEnabled)
            m_forceSync = true;
        if (m_localProbe) {
            m_localClock.start();
            m_localSampleApplied = false;
            const bool requestFresh = !m_bootSampleEligible;
            m_bootSampleEligible = false;
            m_localAuthority = true;
            m_redisFresh = false;
            if (requestFresh) {
                m_localProbeGeneration = m_localProbe->requestSample() - 1;
            } else {
                m_localProbeGeneration = 0;
            }
            m_localProbeTimer->start();
            checkLocalSensor();
        }
        m_pollTimer->start(1000);
        checkBrightness();
    } else {
        m_bootSampleEligible = false;
        m_pollTimer->stop();
        m_localProbeTimer->stop();
        m_lockoutTimer->stop();
        m_pendingSince.invalidate();
    }
}

void AutoThemeService::applyStartupSample()
{
    checkLocalSensor();
}

void AutoThemeService::checkLocalSensor()
{
    if (!m_enabled || !m_localProbe || m_localSampleApplied)
        return;
    if (applyLocalSensor()) {
        m_localProbeTimer->stop();
    } else if (m_localSampleApplied) {
        const bool fallbackReady = !m_localAuthority;
        m_localProbeTimer->stop();
        if (fallbackReady)
            checkBrightness();
    } else if (m_localClock.isValid() && m_localClock.elapsed() >= 1200) {
        m_localClock.invalidate();
        m_localAuthority = false;
        m_redisFresh = true;
        m_localProbeTimer->stop();
        checkBrightness();
    }
}

bool AutoThemeService::applyLocalSensor()
{
    if (!m_localProbe || !m_localProbe->finished()
        || m_localProbe->generation() <= m_localProbeGeneration)
        return false;
    const QString value = m_localProbe->sample();
    bool ok = false;
    const double lux = value.toDouble(&ok);
    if (!ok || !std::isfinite(lux) || lux < 0.0) {
        m_localSampleApplied = true;
        m_localAuthority = false;
        m_redisFresh = true;
        return false;
    }
    m_localSampleApplied = true;
    m_forceSync = true;
    m_localAuthority = true;
    m_redisFresh = false;
    processBrightness(lux);
    return true;
}

void AutoThemeService::checkBrightness()
{
    if (!m_enabled) return;
    checkLocalSensor();
    if (m_localAuthority && !m_redisFresh)
        return;

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

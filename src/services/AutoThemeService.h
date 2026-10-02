#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

class AmbientLightProbe
{
public:
    using Reader = std::function<QString(const QString &)>;

    explicit AmbientLightProbe(QString path = {}, Reader reader = {});
    ~AmbientLightProbe();

    AmbientLightProbe(const AmbientLightProbe &) = delete;
    AmbientLightProbe &operator=(const AmbientLightProbe &) = delete;

    void start();
    quint64 requestSample();
    bool finished() const { return m_finished.load(); }
    QString sample() const;
    quint64 generation() const { return m_generation.load(); }

    static QString defaultPath();

private:
    void run();

    QString m_path;
    Reader m_reader;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    QString m_result;
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_finished{false};
    std::atomic<quint64> m_generation{0};
    bool m_requested = false;
    quint64 m_requestedGeneration = 0;
    bool m_stopping = false;
};

class MdbRepository;
class ThemeStore;

class AutoThemeService : public QObject
{
    Q_OBJECT

public:
    explicit AutoThemeService(MdbRepository *repo, ThemeStore *themeStore,
                              QObject *parent = nullptr);
    ~AutoThemeService() override;

    void setLocalProbe(AmbientLightProbe *probe);
    void setEnabled(bool enabled);
    void applyStartupSample();
    bool isEnabled() const { return m_enabled; }

private slots:
    void checkBrightness();
    void checkLocalSensor();

private:
    void processBrightness(double lux);
    bool applyLocalSensor();
    void commitFlip(bool dark);

    MdbRepository *m_repo;
    ThemeStore *m_themeStore;
    QTimer *m_pollTimer;
    // Lockout after a flip: while active, further flips are suppressed so the
    // theme can't oscillate. Single-shot, started on every committed flip.
    QTimer *m_lockoutTimer;
    QTimer *m_localProbeTimer;
    AmbientLightProbe *m_localProbe = nullptr;
    QElapsedTimer m_localClock;
    quint64 m_localProbeGeneration = 0;
    bool m_localSampleApplied = false;
    bool m_bootSampleEligible = false;
    bool m_localAuthority = false;
    bool m_redisFresh = true;
    quint64 m_dashboardSubscriptionId = 0;
    // How long the reading has been continuously on the far side of the
    // threshold. Invalid means it isn't, and the dwell starts over.
    QElapsedTimer m_pendingSince;
    bool m_enabled = false;
    bool m_currentlyDark = true;
    // Forces the next processBrightness() to push the theme to ThemeStore
    // even if the hysteresis state hasn't changed. Armed on disabled→enabled
    // transition so that re-entering auto mode resyncs the UI (which a
    // manual theme setting in between may have moved away from our cache).
    bool m_forceSync = false;
};

#pragma once

#include <QObject>
#include <QTimer>

class MdbRepository;
class ThemeStore;

class AutoThemeService : public QObject
{
    Q_OBJECT

public:
    explicit AutoThemeService(MdbRepository *repo, ThemeStore *themeStore,
                              QObject *parent = nullptr);
    ~AutoThemeService() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

private slots:
    void checkBrightness();

private:
    void processBrightness(double rawLux);

    MdbRepository *m_repo;
    ThemeStore *m_themeStore;
    QTimer *m_pollTimer;
    // Lockout after a flip: while active, further flips are suppressed so the
    // theme can't oscillate. Single-shot, started on every committed flip.
    QTimer *m_lockoutTimer;
    double m_smoothedBrightness = -1.0;
    bool m_enabled = false;
    bool m_currentlyDark = true;
    // Forces the next processBrightness() to push the theme to ThemeStore
    // even if the hysteresis state hasn't changed. Armed on disabled→enabled
    // transition so that re-entering auto mode resyncs the UI (which a
    // manual theme setting in between may have moved away from our cache).
    bool m_forceSync = false;

    static constexpr double SMOOTHING_ALPHA = 0.7;
    static constexpr double LIGHT_THRESHOLD = 40.0;
    static constexpr double DARK_THRESHOLD = 10.0;
    // Minimum time the theme stays at a level after flipping. Flips happen
    // promptly on a threshold cross; this just blocks an immediate flip back,
    // so the theme can't flicker (e.g. dappled light, dusk boundary).
    static constexpr int LOCKOUT_MS = 10000;
};

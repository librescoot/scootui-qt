#pragma once

#include <QElapsedTimer>
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
    void processBrightness(double lux);
    void commitFlip(bool dark);

    MdbRepository *m_repo;
    ThemeStore *m_themeStore;
    QTimer *m_pollTimer;
    // Lockout after a flip: while active, further flips are suppressed so the
    // theme can't oscillate. Single-shot, started on every committed flip.
    QTimer *m_lockoutTimer;
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

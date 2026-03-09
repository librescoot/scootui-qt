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
    double m_smoothedBrightness = -1.0;
    bool m_enabled = false;
    bool m_currentlyDark = true;

    static constexpr double SMOOTHING_ALPHA = 0.7;
    static constexpr double LIGHT_THRESHOLD = 25.0;
    static constexpr double DARK_THRESHOLD = 15.0;
};

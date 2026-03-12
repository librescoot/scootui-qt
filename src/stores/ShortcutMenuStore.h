#pragma once

#include <QObject>
#include <QTimer>
#include <QDateTime>

class ThemeStore;
class VehicleStore;
class ScreenStore;
class DashboardStore;
class MdbRepository;
class SettingsService;

class ShortcutMenuStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectionChanged)
    Q_PROPERTY(bool confirming READ confirming NOTIFY confirmingChanged)

public:
    explicit ShortcutMenuStore(ThemeStore *theme, VehicleStore *vehicle,
                               ScreenStore *screen, DashboardStore *dashboard,
                               MdbRepository *repo, SettingsService *settingsService,
                               QObject *parent = nullptr);

    bool visible() const { return m_visible; }
    int selectedIndex() const { return m_selectedIndex; }
    bool confirming() const { return m_confirming; }

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void cycle();
    Q_INVOKABLE void confirm();

signals:
    void visibleChanged();
    void selectionChanged();
    void confirmingChanged();

private slots:
    void handleButtonEvent(const QString &channel, const QString &message);
    void onLongPressTimeout();
    void onCycleTimeout();

private:
    void executeAction(int index);
    void toggleHazards();
    void toggleView();
    void toggleDebugOverlay();
    void cycleTheme();
    void resetState();

    ThemeStore *m_theme;
    VehicleStore *m_vehicle;
    ScreenStore *m_screenStore;
    DashboardStore *m_dashboardStore;
    MdbRepository *m_repo;
    SettingsService *m_settingsService;

    QTimer *m_confirmTimer;
    QTimer *m_longPressTimer;
    QTimer *m_cycleTimer;

    bool m_visible = false;
    int m_selectedIndex = 0;
    bool m_confirming = false;

    // Button tracking
    QDateTime m_buttonPressStartTime;
    QDateTime m_lastTapTime;

    static constexpr int ITEM_COUNT = 4;
    static constexpr int DOUBLE_PRESS_MS = 500;
    static constexpr int LONG_PRESS_MS = 500;
    static constexpr int ITEM_CYCLE_MS = 750;
    static constexpr int CONFIRM_TIMEOUT_MS = 1000;
};

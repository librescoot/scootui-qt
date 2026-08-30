#pragma once

#include <QObject>
#include <QTimer>

class ThemeStore;
class VehicleStore;
class Navigator;
class DashboardStore;
class InputHandler;
class CommandBus;
class SettingsService;

class ShortcutMenuStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectionChanged)
    Q_PROPERTY(bool confirming READ confirming NOTIFY confirmingChanged)
    Q_PROPERTY(int confirmTimeoutMs READ confirmTimeoutMs CONSTANT)

public:
    explicit ShortcutMenuStore(ThemeStore *theme, VehicleStore *vehicle,
                               Navigator *screen, DashboardStore *dashboard,
                               InputHandler *input, CommandBus *commands,
                               SettingsService *settingsService,
                               QObject *parent = nullptr);

    bool visible() const { return m_visible; }
    int selectedIndex() const { return m_selectedIndex; }
    bool confirming() const { return m_confirming; }
    int confirmTimeoutMs() const { return CONFIRM_TIMEOUT_MS; }

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void cycle();
    Q_INVOKABLE void confirm();

signals:
    void visibleChanged();
    void selectionChanged();
    void confirmingChanged();

private slots:
    void onCycleTimeout();
    void onSeatboxLongTap();
    void onSeatboxRelease();
    void onSeatboxPress();
    void onSeatboxDoubleTap();

private:
    void executeAction(int index);
    void toggleHazards();
    void toggleView();
    void toggleDebugOverlay();
    void cycleTheme();
    void resetState();
    bool isReadyToDrive() const;

    ThemeStore *m_theme;
    VehicleStore *m_vehicle;
    Navigator *m_navigator;
    DashboardStore *m_dashboardStore;
    CommandBus *m_commands;
    SettingsService *m_settingsService;

    QTimer *m_confirmTimer;
    QTimer *m_cycleTimer;

    bool m_visible = false;
    int m_selectedIndex = 0;
    bool m_confirming = false;

    static constexpr int ITEM_COUNT = 4;
    static constexpr int ITEM_CYCLE_MS = 750;
    static constexpr int CONFIRM_TIMEOUT_MS = 3000;
};

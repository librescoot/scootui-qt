#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class EngineStore;
class VehicleStore;
class ScreenStore;
class SavedLocationsStore;
class NavigationAvailabilityService;
class SettingsStore;
class MdbRepository;
class SettingsService;

class ShortcutMenuStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectionChanged)
    Q_PROPERTY(bool confirming READ confirming NOTIFY confirmingChanged)
    Q_PROPERTY(int confirmTimeoutMs READ confirmTimeoutMs CONSTANT)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY actionsChanged)
    Q_PROPERTY(int actionCount READ actionCount NOTIFY actionsChanged)

public:
    explicit ShortcutMenuStore(EngineStore *engine, VehicleStore *vehicle,
                               ScreenStore *screen, SavedLocationsStore *savedLocations,
                               NavigationAvailabilityService *navigationAvailability,
                               SettingsStore *settings, MdbRepository *repo,
                               SettingsService *settingsService,
                               QObject *parent = nullptr);
    ~ShortcutMenuStore() override;

    bool visible() const { return m_visible; }
    int selectedIndex() const { return m_selectedIndex; }
    bool confirming() const { return m_confirming; }
    int confirmTimeoutMs() const { return CONFIRM_TIMEOUT_MS; }
    QVariantList actions() const { return m_actions; }
    int actionCount() const { return m_actions.size(); }

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void cycle();
    Q_INVOKABLE void confirm();

signals:
    void visibleChanged();
    void selectionChanged();
    void confirmingChanged();
    void actionsChanged();

private slots:
    void onCycleTimeout();
    void rebuildActions();

private:
    void onInputEvent(const QString &message);
    QVariantList availableActions() const;
    void executePendingAction();
    void toggleHazards();
    void toggleView();
    void resetState();
    bool isReadyToDrive() const;
    bool isStationary() const;
    bool destinationAvailable() const;
    static QString actionKey(const QVariantMap &action);

    EngineStore *m_engine;
    VehicleStore *m_vehicle;
    ScreenStore *m_screenStore;
    SavedLocationsStore *m_savedLocations;
    NavigationAvailabilityService *m_navigationAvailability;
    SettingsStore *m_settings;
    MdbRepository *m_repo;
    SettingsService *m_settingsService;
    quint64 m_inputSubscriptionId = 0;

    QTimer *m_confirmTimer;
    QTimer *m_cycleTimer;

    QVariantList m_actions;
    QVariantMap m_pendingAction;
    bool m_visible = false;
    int m_selectedIndex = 0;
    bool m_confirming = false;

    static constexpr int ITEM_CYCLE_MS = 750;
    static constexpr int CONFIRM_TIMEOUT_MS = 3000;
};

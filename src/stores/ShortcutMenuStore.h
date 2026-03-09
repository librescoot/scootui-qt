#pragma once

#include <QObject>
#include <QTimer>

class ThemeStore;
class SettingsService;

class ShortcutMenuStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectionChanged)
    Q_PROPERTY(bool confirming READ confirming NOTIFY confirmingChanged)

public:
    explicit ShortcutMenuStore(ThemeStore *theme, SettingsService *settingsService,
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

private:
    void resetConfirmation();

    ThemeStore *m_theme;
    SettingsService *m_settingsService;
    QTimer *m_confirmTimer;
    bool m_visible = false;
    int m_selectedIndex = 0;
    bool m_confirming = false;
    static constexpr int ITEM_COUNT = 3;
};

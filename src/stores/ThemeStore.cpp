#include "ThemeStore.h"
#include "SettingsStore.h"

ThemeStore::ThemeStore(SettingsStore *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    s_instance = this;
    connect(m_settings, &SettingsStore::themeChanged,
            this, &ThemeStore::onSettingsThemeChanged);
    onSettingsThemeChanged();
}

void ThemeStore::onSettingsThemeChanged()
{
    const QString theme = m_settings->theme();
    bool autoMode = (theme == QLatin1String("auto"));
    bool dark = (theme != QLatin1String("light"));

    bool changed = false;
    if (autoMode != m_isAutoMode) {
        m_isAutoMode = autoMode;
        changed = true;
    }
    // In auto mode, don't change dark/light here - AutoThemeService handles it
    if (!autoMode && dark != m_isDark) {
        m_isDark = dark;
        changed = true;
    }
    if (changed)
        emit themeChanged();
}

void ThemeStore::setTheme(const QString &theme)
{
    bool dark = (theme != QLatin1String("light"));
    if (dark != m_isDark) {
        m_isDark = dark;
        emit themeChanged();
    }
}

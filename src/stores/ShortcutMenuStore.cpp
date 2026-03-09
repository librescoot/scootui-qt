#include "ShortcutMenuStore.h"
#include "ThemeStore.h"
#include "services/SettingsService.h"

ShortcutMenuStore::ShortcutMenuStore(ThemeStore *theme, SettingsService *settingsService,
                                     QObject *parent)
    : QObject(parent)
    , m_theme(theme)
    , m_settingsService(settingsService)
    , m_confirmTimer(new QTimer(this))
{
    m_confirmTimer->setSingleShot(true);
    m_confirmTimer->setInterval(1000);
    connect(m_confirmTimer, &QTimer::timeout, this, &ShortcutMenuStore::resetConfirmation);
}

void ShortcutMenuStore::show()
{
    if (!m_visible) {
        m_visible = true;
        m_selectedIndex = 0;
        m_confirming = false;
        emit visibleChanged();
        emit selectionChanged();
    }
}

void ShortcutMenuStore::hide()
{
    if (m_visible) {
        m_visible = false;
        m_confirming = false;
        m_confirmTimer->stop();
        emit visibleChanged();
    }
}

void ShortcutMenuStore::cycle()
{
    if (m_confirming) {
        resetConfirmation();
    }
    m_selectedIndex = (m_selectedIndex + 1) % ITEM_COUNT;
    emit selectionChanged();
}

void ShortcutMenuStore::confirm()
{
    m_confirming = true;
    emit confirmingChanged();
    m_confirmTimer->start();

    switch (m_selectedIndex) {
    case 0: // Toggle theme: auto -> dark -> light -> auto
    {
        // Cycle theme
        if (m_theme->isDark()) {
            m_settingsService->updateTheme(QStringLiteral("light"));
        } else {
            m_settingsService->updateTheme(QStringLiteral("dark"));
        }
        break;
    }
    case 1: // Toggle view (cluster/map) - placeholder
        break;
    case 2: // Toggle hazards - placeholder
        break;
    }
}

void ShortcutMenuStore::resetConfirmation()
{
    if (m_confirming) {
        m_confirming = false;
        emit confirmingChanged();
    }
}

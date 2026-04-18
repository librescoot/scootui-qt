#include "LocaleStore.h"
#include "SettingsStore.h"

LocaleStore::LocaleStore(SettingsStore *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    s_instance = this;
    connect(m_settings, &SettingsStore::languageChanged,
            this, &LocaleStore::onSettingsLanguageChanged);
    onSettingsLanguageChanged();
}

void LocaleStore::onSettingsLanguageChanged()
{
    const QString lang = m_settings->language();
    if (!lang.isEmpty() && lang != m_language) {
        m_language = lang;
        emit languageChanged();
    }
}

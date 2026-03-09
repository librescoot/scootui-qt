#pragma once

#include <QObject>
#include <QLocale>

class SettingsStore;

class LocaleStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(QString locale READ locale NOTIFY languageChanged)

public:
    explicit LocaleStore(SettingsStore *settings, QObject *parent = nullptr);

    QString language() const { return m_language; }
    QString locale() const { return m_language == QLatin1String("de") ? QStringLiteral("de_DE") : QStringLiteral("en_US"); }

signals:
    void languageChanged();

private slots:
    void onSettingsLanguageChanged();

private:
    SettingsStore *m_settings;
    QString m_language = QStringLiteral("en");
};

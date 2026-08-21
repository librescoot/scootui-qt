#pragma once

#include <QObject>
#include <QLocale>
#include <QtQml/qqmlengine.h>

class SettingsStore;

class LocaleStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
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

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static LocaleStore *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(LocaleStore *instance) { s_qmlInstance = instance; }

private:
    static inline LocaleStore *s_qmlInstance = nullptr;
};

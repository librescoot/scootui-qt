#pragma once

#include <QObject>
#include "models/Enums.h"
#include <QtQml/qqmlregistration.h>

class SettingsStore;

class QQmlEngine;
class QJSEngine;

class ScreenStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int currentScreen READ currentScreen NOTIFY currentScreenChanged)

public:
    explicit ScreenStore(SettingsStore *settings, QObject *parent = nullptr);
    static ScreenStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    int currentScreen() const { return static_cast<int>(m_currentScreen); }
    ScootEnums::ScreenMode currentScreenMode() const { return m_currentScreen; }

    Q_PROPERTY(int setupMode READ setupMode NOTIFY setupModeChanged)

    Q_INVOKABLE void setScreen(int screen);
    Q_INVOKABLE void showAbout();
    Q_INVOKABLE void closeAbout();
    Q_INVOKABLE void showNavigationSetup(int setupMode = 2);
    Q_INVOKABLE void closeNavigationSetup();

    int setupMode() const { return m_setupMode; }

signals:
    void currentScreenChanged();
    void setupModeChanged();

private:
    void applyMode(const QString &mode);

    ScootEnums::ScreenMode m_currentScreen = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeAbout = ScootEnums::ScreenMode::Cluster;
    ScootEnums::ScreenMode m_screenBeforeNavSetup = ScootEnums::ScreenMode::Cluster;
    int m_setupMode = 2; // Both by default

    static inline ScreenStore *s_instance = nullptr;
};

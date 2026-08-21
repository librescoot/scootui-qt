#pragma once

#include "SyncableStore.h"
#include <QtQml/qqmlengine.h>

class DashboardStore : public SyncableStore
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString debugMode READ debugMode NOTIFY debugModeChanged)
    // Ambient illuminance in lux and the resulting backlight level, both from
    // the `dashboard` hash (dbc-backlight-service). <0 means not yet reported.
    Q_PROPERTY(double brightness READ brightness NOTIFY brightnessChanged)
    Q_PROPERTY(int backlight READ backlight NOTIFY backlightChanged)

public:
    explicit DashboardStore(MdbRepository *repo, QObject *parent = nullptr);

    QString debugMode() const { return m_debugMode; }
    double brightness() const { return m_brightness; }
    int backlight() const { return m_backlight; }

    Q_INVOKABLE void setBacklightEnabled(bool enabled);

signals:
    void debugModeChanged();
    void brightnessChanged();
    void backlightChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_debugMode = QStringLiteral("off");
    double m_brightness = -1.0;
    int m_backlight = -1;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static DashboardStore *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(DashboardStore *instance) { s_qmlInstance = instance; }

private:
    static inline DashboardStore *s_qmlInstance = nullptr;
};

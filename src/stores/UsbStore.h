#pragma once

#include "SyncableStore.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class UsbStore : public SyncableStore
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString step READ step NOTIFY stepChanged)

public:
    explicit UsbStore(MdbRepository *repo, QObject *parent = nullptr);
    static UsbStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    QString status() const { return m_status; }
    QString mode() const { return m_mode; }
    QString step() const { return m_step; }

    Q_INVOKABLE void exitUmsMode();

signals:
    void statusChanged();
    void modeChanged();
    void stepChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_status = QStringLiteral("idle");
    QString m_mode = QStringLiteral("normal");
    QString m_step;
    static inline UsbStore *s_instance = nullptr;
};

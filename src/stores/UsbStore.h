#pragma once

#include "SyncableStore.h"
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class UsbStore : public SyncableStore
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString step READ step NOTIFY stepChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)

public:
    explicit UsbStore(MdbRepository *repo, QObject *parent = nullptr);
    static UsbStore *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    QString status() const { return m_status; }
    QString mode() const { return m_mode; }
    QString step() const { return m_step; }
    int progress() const { return m_progress; }
    QString detail() const { return m_detail; }

    Q_INVOKABLE void exitUmsMode();

signals:
    void statusChanged();
    void modeChanged();
    void stepChanged();
    void progressChanged();
    void detailChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_status = QStringLiteral("idle");
    QString m_mode = QStringLiteral("normal");
    QString m_step;
    int m_progress = 0;
    QString m_detail;

    static inline UsbStore *s_instance = nullptr;
};

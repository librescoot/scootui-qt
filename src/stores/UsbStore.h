#pragma once

#include "SyncableStore.h"

class UsbStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)

public:
    explicit UsbStore(MdbRepository *repo, QObject *parent = nullptr);

    QString status() const { return m_status; }
    QString mode() const { return m_mode; }

signals:
    void statusChanged();
    void modeChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_status = QStringLiteral("idle");
    QString m_mode = QStringLiteral("normal");
};

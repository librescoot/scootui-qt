#pragma once

#include "SyncableStore.h"

class AutoStandbyStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(int autoStandbyRemaining READ autoStandbyRemaining NOTIFY autoStandbyRemainingChanged)

public:
    explicit AutoStandbyStore(MdbRepository *repo, QObject *parent = nullptr);

    int autoStandbyRemaining() const { return m_autoStandbyRemaining; }

signals:
    void autoStandbyRemainingChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    int m_autoStandbyRemaining = 0;
};

#pragma once

#include <QVariantMap>
#include <QVariantList>
#include <QList>

struct AttentionSelection {
    QVariantMap main;
    QVariantMap companion;
    int criticalCount = 0;
};

struct AttentionCycleState {
    // Registry order identifies an entry lifetime and survives producer revisions.
    quint64 order = 0;
    int priority = -1;
    qint64 presentedAtMs = 0;
};

class AttentionPolicy
{
public:
    static constexpr qint64 DwellMs = 5000;

    static AttentionSelection select(const QList<QVariantMap> &conditions,
                                     const QList<QVariantMap> &events,
                                     const QVariantMap &navigation,
                                     bool riding, qint64 nowMs,
                                     AttentionCycleState *cycle = nullptr);
};

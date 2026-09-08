#pragma once

#include <QVariantMap>
#include <QVariantList>
#include <QList>

struct AttentionSelection {
    QVariantMap main;
    QVariantMap companion;
    int criticalCount = 0;
};

class AttentionPolicy
{
public:
    static AttentionSelection select(const QList<QVariantMap> &conditions,
                                     const QList<QVariantMap> &events,
                                     const QVariantMap &navigation,
                                     bool riding, qint64 nowMs);
};

#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace RoadSignStyle {

inline QString classify(const QString &roadType, const QString &roadRefs,
                        const QString &routeNetworks)
{
    if (roadType.compare(QStringLiteral("motorway"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("motorway");

    const QStringList networks = routeNetworks.split(
        QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &network : networks) {
        if (network.trimmed() == QLatin1String("DE:national"))
            return QStringLiteral("federal");
    }

    static const QRegularExpression federalRef(
        QStringLiteral("^B\\s*\\d"),
        QRegularExpression::CaseInsensitiveOption);
    const QStringList refs = roadRefs.split(
        QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts);
    for (const QString &ref : refs) {
        if (federalRef.match(ref.trimmed()).hasMatch())
            return QStringLiteral("federal");
    }

    return {};
}

}

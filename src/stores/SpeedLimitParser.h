#pragma once

#include <QRegularExpression>
#include <QString>

#include <cmath>

namespace SpeedLimitParser {

inline QString resolve(const QString &rawValue)
{
    const QString raw = rawValue.trimmed();
    if (raw.isEmpty())
        return raw;

    bool ok = false;
    const int integer = raw.toInt(&ok);
    if (ok)
        return QString::number(integer);
    if (raw == QLatin1String("none"))
        return raw;

    if (raw == QLatin1String("DE:urban"))          return QStringLiteral("50");
    if (raw == QLatin1String("DE:rural"))          return QStringLiteral("100");
    if (raw == QLatin1String("DE:motorway"))       return QStringLiteral("none");
    if (raw == QLatin1String("DE:living_street"))  return QStringLiteral("10");
    if (raw == QLatin1String("DE:walk"))           return QStringLiteral("5");
    if (raw == QLatin1String("DE:zone30")
        || raw == QLatin1String("DE:zone:30"))     return QStringLiteral("30");
    if (raw == QLatin1String("DE:zone20")
        || raw == QLatin1String("DE:zone:20"))     return QStringLiteral("20");
    if (raw == QLatin1String("DE:bicycle_road"))   return QStringLiteral("30");

    if (raw.contains(QLatin1Char(';')))
        return resolve(raw.section(QLatin1Char(';'), 0, 0));

    static const QRegularExpression mph(
        QStringLiteral("^(\\d+(?:\\.\\d+)?)\\s*mph$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto mphMatch = mph.match(raw);
    if (mphMatch.hasMatch()) {
        const double value = mphMatch.captured(1).toDouble();
        return QString::number(static_cast<int>(std::lround(value * 1.609344)));
    }

    static const QRegularExpression kph(
        QStringLiteral("^(\\d+(?:\\.\\d+)?)\\s*(?:km/h|kph)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto kphMatch = kph.match(raw);
    if (kphMatch.hasMatch())
        return QString::number(static_cast<int>(std::lround(
            kphMatch.captured(1).toDouble())));

    return {};
}

} // namespace SpeedLimitParser

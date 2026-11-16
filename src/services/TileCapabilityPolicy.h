#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <cmath>
#include <limits>

// Evaluates v2 policy against capabilities of the consuming DBC build.
// Callers must not obtain capabilities from the feed and must reject duplicate
// JSON keys before constructing the QJsonObject.
class TileCapabilityPolicy
{
public:
    static bool supports(const QJsonObject &asset, const QString &kind, const QString &region,
                         const QSet<QString> &capabilities,
                         const QString &targetDigest = {}, const QString &baseDigest = {})
    {
        static const QRegularExpression digest(QStringLiteral("^[0-9a-f]{64}$"));
        static const QRegularExpression token(QStringLiteral("^[a-z0-9][a-z0-9.-]*$"));
        static const QSet<QString> kinds{QStringLiteral("map"), QStringLiteral("routing"),
                                         QStringLiteral("map-delta"), QStringLiteral("routing-delta"),
                                         QStringLiteral("routing-compressed")};
        if (!kinds.contains(kind) || region.isEmpty()
            || asset.value(QStringLiteral("kind")).toString() != kind
            || asset.value(QStringLiteral("region")).toString() != region)
            return false;
        const auto hash = asset.value(QStringLiteral("sha256")).toString();
        const auto format = asset.value(QStringLiteral("format")).toString();
        const double size = asset.value(QStringLiteral("size")).toDouble();
        const QUrl url(asset.value(QStringLiteral("url")).toString());
        if (!digest.match(hash).hasMatch() || !token.match(format).hasMatch()
            || !std::isfinite(size) || size <= 0 || size >= double(std::numeric_limits<qint64>::max())
            || std::floor(size) != size || !url.isValid() || url.scheme() != QLatin1String("https")
            || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment())
            return false;
        const auto declared = asset.value(QStringLiteral("requires"));
        if (!declared.isArray() || declared.toArray().isEmpty()) return false;
        QSet<QString> required;
        for (const auto &value : declared.toArray()) {
            if (!value.isString()) return false;
            const auto item = value.toString();
            if (!token.match(item).hasMatch() || required.contains(item) || !capabilities.contains(item))
                return false;
            required.insert(item);
        }
        if (!required.contains(kind + QStringLiteral(".format.") + format)) return false;
        const bool delta = kind == QLatin1String("map-delta") || kind == QLatin1String("routing-delta");
        const bool compressed = kind == QLatin1String("routing-compressed");
        if (delta || compressed) {
            if (!digest.match(targetDigest).hasMatch()
                || asset.value(QStringLiteral("target_sha256")).toString() != targetDigest)
                return false;
        }
        if (compressed && asset.value(QStringLiteral("codec")).toString() != format) return false;
        if (delta) {
            const auto algorithm = asset.value(QStringLiteral("algorithm")).toString();
            if (!token.match(algorithm).hasMatch() || !required.contains(QStringLiteral("patch.algorithm.") + algorithm)
                || !digest.match(baseDigest).hasMatch()
                || asset.value(QStringLiteral("base_sha256")).toString() != baseDigest
                || asset.value(QStringLiteral("base_version")).toString().isEmpty())
                return false;
        }
        return true;
    }
};

#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <optional>

struct MapTileInfo {
    QString digest;
    QString publishedAt;
    qint64 size = 0;

    QJsonObject toJson() const {
        QJsonObject o;
        o[QStringLiteral("digest")] = digest;
        o[QStringLiteral("publishedAt")] = publishedAt;
        o[QStringLiteral("size")] = size;
        return o;
    }

    static MapTileInfo fromJson(const QJsonObject &o) {
        MapTileInfo info;
        info.digest = o[QStringLiteral("digest")].toString();
        info.publishedAt = o[QStringLiteral("publishedAt")].toString();
        info.size = o[QStringLiteral("size")].toDouble();
        return info;
    }
};

struct MapMetadata {
    QString region;
    std::optional<MapTileInfo> displayTiles;
    std::optional<MapTileInfo> valhallaTiles;
    QString lastUpdateCheck;
    bool updateAvailable = false;

    QJsonObject toJson() const {
        QJsonObject o;
        o[QStringLiteral("region")] = region;
        if (displayTiles)
            o[QStringLiteral("displayTiles")] = displayTiles->toJson();
        if (valhallaTiles)
            o[QStringLiteral("valhallaTiles")] = valhallaTiles->toJson();
        if (!lastUpdateCheck.isEmpty())
            o[QStringLiteral("lastUpdateCheck")] = lastUpdateCheck;
        if (updateAvailable)
            o[QStringLiteral("updateAvailable")] = true;
        return o;
    }

    static MapMetadata fromJson(const QJsonObject &o) {
        MapMetadata m;
        m.region = o[QStringLiteral("region")].toString();
        if (o.contains(QStringLiteral("displayTiles")))
            m.displayTiles = MapTileInfo::fromJson(o[QStringLiteral("displayTiles")].toObject());
        if (o.contains(QStringLiteral("valhallaTiles")))
            m.valhallaTiles = MapTileInfo::fromJson(o[QStringLiteral("valhallaTiles")].toObject());
        m.lastUpdateCheck = o[QStringLiteral("lastUpdateCheck")].toString();
        m.updateAvailable = o[QStringLiteral("updateAvailable")].toBool();
        return m;
    }

    static QString metadataPath() {
#ifdef Q_OS_LINUX
        return QStringLiteral("/data/maps/metadata.json");
#else
        return QDir::homePath() + QStringLiteral("/.local/share/scootui/metadata.json");
#endif
    }

    static MapMetadata load() {
        QFile f(metadataPath());
        if (!f.open(QIODevice::ReadOnly))
            return {};
        auto doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isNull())
            return {};
        return fromJson(doc.object());
    }

    static bool save(const MapMetadata &meta) {
        QString path = metadataPath();
        QDir().mkpath(QFileInfo(path).absolutePath());

        // QSaveFile writes to a temp file and only replaces the target on a
        // successful commit(), so metadata.json is never seen truncated or
        // half-written.
        //
        // That is integrity, not durability: QSaveFile does not fsync, so a
        // power cut shortly after a write can revert the file to its previous
        // contents entirely. Survivable here — a stale or missing metadata.json
        // costs a re-check, not correctness — but do not copy this pattern for
        // state that must not be lost. See OdometerMilestoneService's
        // writeFileAtomic, or modem-service's data-usage counter, for the
        // fsync-file-then-fsync-directory version.
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return false;
        f.write(QJsonDocument(meta.toJson()).toJson(QJsonDocument::Compact));
        return f.commit();
    }
};

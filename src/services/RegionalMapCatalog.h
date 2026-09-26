#pragma once

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>

// Installed packs are paired by slug. Bounds come from the display MBTiles;
// a missing routing archive does not prevent displaying a region.
class RegionalMapCatalog
{
public:
    struct Pack {
        QString slug;
        QString mapPath;
        QString routingPath;
        double west = 0, south = 0, east = 0, north = 0;

        bool contains(double lat, double lon, double margin = 0) const {
            return lat >= south + margin && lat <= north - margin
                && lon >= west + margin && lon <= east - margin;
        }
        double area() const { return (east - west) * (north - south); }
    };

    static QVector<Pack> scan(const QString &mapsDir = QStringLiteral("/data/maps"),
                              const QString &routingDir = QStringLiteral("/data/valhalla"))
    {
        QVector<Pack> packs;
        const QDir dir(mapsDir);
        for (const QString &filename : dir.entryList({QStringLiteral("tiles_*.mbtiles")}, QDir::Files)) {
            const QString slug = filename.mid(6, filename.size() - 14);
            if (slug.isEmpty())
                continue;
            Pack pack;
            pack.slug = slug;
            pack.mapPath = dir.filePath(filename);
            pack.routingPath = QDir(routingDir).filePath(QStringLiteral("valhalla_tiles_") + slug
                                                          + QStringLiteral(".tar"));
            if (!QFileInfo::exists(pack.routingPath))
                pack.routingPath.clear();
            const QString connection = QStringLiteral("region_catalog_%1")
                .arg(reinterpret_cast<quintptr>(&pack), 0, 16);
            bool valid = false;
            {
                auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                db.setDatabaseName(pack.mapPath);
                db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
                if (db.open()) {
                    QSqlQuery query(db);
                    if (query.exec(QStringLiteral("SELECT value FROM metadata WHERE name='bounds'"))
                        && query.next()) {
                        const auto parts = query.value(0).toString().split(QLatin1Char(','));
                        bool ok[4] = {};
                        if (parts.size() == 4) {
                            pack.west = parts[0].toDouble(&ok[0]);
                            pack.south = parts[1].toDouble(&ok[1]);
                            pack.east = parts[2].toDouble(&ok[2]);
                            pack.north = parts[3].toDouble(&ok[3]);
                            valid = std::all_of(std::begin(ok), std::end(ok), [](bool b) { return b; })
                                && pack.west < pack.east && pack.south < pack.north;
                        }
                    }
                    db.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            if (valid)
                packs.append(pack);
        }
        return packs;
    }

    static const Pack *select(const QVector<Pack> &packs, double lat, double lon,
                              const QString &current = {})
    {
        // The current pack wins in the overlap, avoiding oscillation near borders.
        for (const auto &pack : packs)
            if (pack.slug == current && pack.contains(lat, lon))
                return &pack;
        const Pack *best = nullptr;
        for (const auto &pack : packs) {
            if (pack.contains(lat, lon) && (!best || pack.area() < best->area()))
                best = &pack;
        }
        return best;
    }
};

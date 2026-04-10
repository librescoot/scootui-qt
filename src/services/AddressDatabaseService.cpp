#include "AddressDatabaseService.h"
#include "VectorTileDecoder.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QtMath>
#include <algorithm>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

const QString AddressDatabaseService::MbtilesPath = QStringLiteral("/data/maps/map.mbtiles");
const QString AddressDatabaseService::CachePath = QStringLiteral("/data/scootui/address_database.json");

AddressDatabaseService::AddressDatabaseService(QObject *parent)
    : QObject(parent)
{
}

AddressDatabaseService::~AddressDatabaseService() = default;


// ---------------------------------------------------------------------------
// City name cleanup: strip district suffixes, fix casing
// ---------------------------------------------------------------------------

QString AddressDatabaseService::cleanCityName(const QString &raw)
{
    QString city = raw.trimmed();
    if (city.isEmpty())
        return city;

    // Normalize en-dash (U+2013) and em-dash (U+2014) to regular hyphen
    city.replace(QChar(0x2013), QLatin1Char('-'));
    city.replace(QChar(0x2014), QLatin1Char('-'));

    // "Berlin - Hellersdorf" → "Berlin"
    int dashIdx = city.indexOf(QStringLiteral(" - "));
    if (dashIdx > 0)
        city = city.left(dashIdx).trimmed();

    // Title-case: "berlin" → "Berlin", "BERLIN" → "Berlin"
    // Preserve internal casing for multi-word cities like "Frankfurt am Main"
    bool afterSpace = true;
    for (int i = 0; i < city.size(); ++i) {
        if (city[i] == QLatin1Char(' ') || city[i] == QLatin1Char('-')) {
            afterSpace = true;
        } else if (afterSpace) {
            city[i] = city[i].toUpper();
            afterSpace = false;
        } else {
            city[i] = city[i].toLower();
        }
    }

    return city;
}

// ---------------------------------------------------------------------------
// Normalization: fold umlauts, strip diacritics, lowercase
// ---------------------------------------------------------------------------

QString AddressDatabaseService::normalize(const QString &name)
{
    QString result;
    result.reserve(name.size() + 8);

    for (int i = 0; i < name.size(); ++i) {
        ushort u = name[i].unicode();
        switch (u) {
        case 0x00C4: case 0x00E4: result += QLatin1String("ae"); break; // Ää
        case 0x00D6: case 0x00F6: result += QLatin1String("oe"); break; // Öö
        case 0x00DC: case 0x00FC: result += QLatin1String("ue"); break; // Üü
        case 0x00DF: result += QLatin1String("ss"); break;               // ß
        default:
            if (name[i].isLetterOrNumber()) {
                result += name[i].toLower();
            } else if (name[i] == QLatin1Char(' ') || name[i] == QLatin1Char('-')) {
                result += name[i];
            }
            // drop dots and other special characters
            break;
        }
    }

    // Expand common German street abbreviations so "Str" and "Straße" match
    result.replace(QStringLiteral("str "), QStringLiteral("strasse "));
    if (result.endsWith(QStringLiteral("str")))
        result.replace(result.size() - 3, 3, QStringLiteral("strasse"));
    result.replace(QStringLiteral("pl "), QStringLiteral("platz "));
    if (result.endsWith(QStringLiteral("pl")))
        result.replace(result.size() - 2, 2, QStringLiteral("platz"));

    return result;
}

// ---------------------------------------------------------------------------
// City trie queries
// ---------------------------------------------------------------------------

static QStringList charListToSortedStrings(const QList<QChar> &chars)
{
    QStringList out;
    out.reserve(chars.size());
    for (QChar c : chars)
        out.append(QString(c));
    out.sort();
    return out;
}

QStringList AddressDatabaseService::getValidCityChars(const QString &prefix) const
{
    if (m_status != Ready)
        return {};
    uint32_t node = m_cityTrie.findNode(normalize(prefix));
    if (!m_cityTrie.valid(node))
        return {};
    return charListToSortedStrings(m_cityTrie.childChars(node));
}

int AddressDatabaseService::getCityCount(const QString &prefix) const
{
    if (m_status != Ready)
        return 0;
    uint32_t node = m_cityTrie.findNode(normalize(prefix));
    if (!m_cityTrie.valid(node))
        return 0;
    return m_cityTrie.subtreeUniqueCount(node);
}

QStringList AddressDatabaseService::getMatchingCities(const QString &prefix) const
{
    if (m_status != Ready)
        return {};
    uint32_t node = m_cityTrie.findNode(normalize(prefix));
    if (!m_cityTrie.valid(node))
        return {};
    QStringList result = m_cityTrie.collectDisplayNames(node);
    result.sort(Qt::CaseInsensitive);
    return result;
}

// ---------------------------------------------------------------------------
// Street trie queries (within a city)
// ---------------------------------------------------------------------------

QStringList AddressDatabaseService::getValidStreetChars(const QString &city, const QString &prefix) const
{
    if (m_status != Ready)
        return {};
    QString normCity = normalize(city);
    auto trieIt = m_streetTries.constFind(normCity);
    if (trieIt == m_streetTries.constEnd() || !trieIt.value())
        return {};
    const FlatTrie *trie = trieIt.value().get();
    uint32_t node = trie->findNode(normalize(prefix));
    if (!trie->valid(node))
        return {};
    return charListToSortedStrings(trie->childChars(node));
}

int AddressDatabaseService::getStreetCount(const QString &city, const QString &prefix) const
{
    if (m_status != Ready)
        return 0;
    QString normCity = normalize(city);
    auto trieIt = m_streetTries.constFind(normCity);
    if (trieIt == m_streetTries.constEnd() || !trieIt.value())
        return 0;
    const FlatTrie *trie = trieIt.value().get();
    uint32_t node = trie->findNode(normalize(prefix));
    if (!trie->valid(node))
        return 0;
    return trie->subtreeUniqueCount(node);
}

QVariantList AddressDatabaseService::getMatchingStreets(const QString &city, const QString &prefix) const
{
    if (m_status != Ready)
        return {};
    QString normCity = normalize(city);
    auto trieIt = m_streetTries.constFind(normCity);
    if (trieIt == m_streetTries.constEnd() || !trieIt.value())
        return {};
    const FlatTrie *trie = trieIt.value().get();
    uint32_t node = trie->findNode(normalize(prefix));
    if (!trie->valid(node))
        return {};

    // Collect display names from trie terminals
    QStringList streetNames = trie->collectDisplayNames(node);

    // Look up postcodes from m_streetData for disambiguation
    auto cityDataIt = m_streetData.constFind(normCity);
    QList<QPair<QString, QString>> pairs;
    for (const QString &name : streetNames) {
        if (cityDataIt != m_streetData.constEnd()) {
            auto streetIt = cityDataIt.value().constFind(normalize(name));
            if (streetIt != cityDataIt.value().constEnd()) {
                const auto &rec = streetIt.value();
                int n = rec.postcodeCount();
                if (n > 1) {
                    // Multiple postcodes — create one entry per postcode
                    pairs.append({name, rec.firstPostcode});
                    for (const auto &e : rec.extraPcs)
                        pairs.append({name, e.first});
                    continue;
                } else if (n == 1) {
                    pairs.append({name, rec.firstPostcode});
                    continue;
                }
            }
        }
        pairs.append({name, QString()});
    }
    std::sort(pairs.begin(), pairs.end(), [](const auto &a, const auto &b) {
        int cmp = a.first.compare(b.first, Qt::CaseInsensitive);
        return cmp != 0 ? cmp < 0 : a.second < b.second;
    });

    QVariantList result;
    for (const auto &pair : pairs) {
        QVariantMap entry;
        entry[QStringLiteral("street")] = pair.first;
        entry[QStringLiteral("postcode")] = pair.second;
        result.append(entry);
    }
    return result;
}

// ---------------------------------------------------------------------------
// House number and coordinate queries
// ---------------------------------------------------------------------------

void AddressDatabaseService::queryHouseNumbers(const QString &city, const QString &street, const QString &postcode)
{
    if (m_status != Ready) {
        emit houseNumbersReady({});
        return;
    }
    QString normCity = normalize(city);
    QString normStreet = normalize(street);

    auto cityIt = m_streetData.constFind(normCity);
    if (cityIt == m_streetData.constEnd()) {
        emit houseNumbersReady({});
        return;
    }
    auto streetIt = cityIt.value().constFind(normStreet);
    if (streetIt == cityIt.value().constEnd()) {
        emit houseNumbersReady({});
        return;
    }

    // Use postcode-specific centroid if available, otherwise overall
    const auto &rec = streetIt.value();
    double lat = rec.centroid.lat;
    double lng = rec.centroid.lng;
    if (!postcode.isEmpty()) {
        if (const auto *pcd = rec.findPcCentroid(postcode)) {
            lat = pcd->lat;
            lng = pcd->lng;
        }
    }

    // Run the tile query on a background thread to avoid blocking the UI
    auto *watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher]() {
        emit houseNumbersReady(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([this, city, street, postcode, lat, lng]() {
        return queryHouseNumbersFromTiles(city, street, postcode, lat, lng);
    }));
}

QVariantMap AddressDatabaseService::getStreetCoordinates(const QString &city, const QString &street) const
{
    if (m_status != Ready)
        return {};
    QString normCity = normalize(city);
    QString normStreet = normalize(street);

    auto cityIt = m_streetData.constFind(normCity);
    if (cityIt == m_streetData.constEnd())
        return {};
    auto streetIt = cityIt.value().constFind(normStreet);
    if (streetIt == cityIt.value().constEnd())
        return {};

    QVariantMap result;
    result[QStringLiteral("latitude")] = streetIt.value().centroid.lat;
    result[QStringLiteral("longitude")] = streetIt.value().centroid.lng;
    return result;
}

// ---------------------------------------------------------------------------
// On-demand house number lookup from mbtiles
// ---------------------------------------------------------------------------

static int lonToTileX_query(double lon, int zoom)
{
    return static_cast<int>(std::floor((lon + 180.0) / 360.0 * (1 << zoom)));
}

static int latToTileYTMS_query(double lat, int zoom)
{
    double latRad = lat * M_PI / 180.0;
    double n = std::pow(2.0, zoom);
    double y = (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * n;
    // Convert slippy Y to TMS Y: MBTiles tile_row uses TMS (Y=0 at bottom)
    return static_cast<int>(n) - 1 - static_cast<int>(std::floor(y));
}

QVariantList AddressDatabaseService::queryHouseNumbersFromTiles(
    const QString &city, const QString &street, const QString &postcode,
    double nearLat, double nearLng) const
{
    qDebug() << "queryHouseNumbers: city=" << city << "street=" << street
             << "postcode=" << postcode << "near=" << nearLat << nearLng;

    QString mbtilesPath = QFile::exists(QStringLiteral("map.mbtiles"))
        ? QStringLiteral("map.mbtiles")
        : MbtilesPath;

    if (!QFile::exists(mbtilesPath))
        return {};

    // Open database (read-only, separate connection)
    const QString connName = QStringLiteral("address_housenumber_query");
    QVariantList result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(mbtilesPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open()) {
            QSqlDatabase::removeDatabase(connName);
            return {};
        }

        constexpr int zoom = 14;
        constexpr double n = 16384.0; // 2^14

        // Scan tiles in a radius around the street centroid (~2km at zoom 14)
        int centerX = lonToTileX_query(nearLng, zoom);
        int centerY = latToTileYTMS_query(nearLat, zoom);
        constexpr int radius = 3; // tiles in each direction

        QString normStreet = normalize(street);
        QSet<QString> seenNumbers;
        int tilesFound = 0, totalAddresses = 0, streetMatches = 0, cityMatches = 0;

        qDebug() << "queryHouseNumbers: normStreet=" << normStreet
                 << "tileCenter=" << centerX << centerY;

        QSqlQuery tileQuery(db);
        tileQuery.prepare(QStringLiteral(
            "SELECT tile_data FROM tiles WHERE zoom_level=14 AND tile_column=? AND tile_row=?"));

        for (int x = centerX - radius; x <= centerX + radius; ++x) {
            for (int y = centerY - radius; y <= centerY + radius; ++y) {
                tileQuery.bindValue(0, x);
                tileQuery.bindValue(1, y);
                if (!tileQuery.exec() || !tileQuery.next())
                    continue;

                tilesFound++;
                QByteArray tileData = tileQuery.value(0).toByteArray();
                QByteArray decompressed = VectorTile::gunzip(tileData);
                if (decompressed.isEmpty())
                    continue;

                VectorTile::Tile tile = VectorTile::parse(decompressed);

                for (const auto &layer : tile.layers) {
                    if (layer.name != QLatin1String("addresses"))
                        continue;

                    for (const auto &feature : layer.features) {
                        if (feature.type != 1)
                            continue;

                        totalAddresses++;
                        QString fStreet = feature.properties.value(QStringLiteral("street"));
                        if (fStreet.isEmpty())
                            fStreet = feature.properties.value(QStringLiteral("name"));
                        QString normFS = normalize(fStreet);
                        if (normFS != normStreet) {
                            if (totalAddresses <= 3)
                                qDebug() << "  sample street:" << fStreet << "→" << normFS;
                            continue;
                        }
                        streetMatches++;

                        // City match: the tile may have "Berlin-Hellersdorf" but user
                        // selected merged city "Berlin", so check if normalized tile city
                        // starts with the target (handles district suffixes)
                        QString fCity = cleanCityName(feature.properties.value(QStringLiteral("city")));
                        QString normFCity = normalize(fCity);
                        QString normTargetCity = normalize(city);
                        if (normFCity != normTargetCity && !normFCity.startsWith(normTargetCity + QLatin1Char('-')))
                            continue;

                        QString fPostcode = feature.properties.value(QStringLiteral("postcode"));
                        if (!postcode.isEmpty() && fPostcode != postcode)
                            continue;

                        QString hn = feature.properties.value(QStringLiteral("housenumber"));
                        if (hn.isEmpty())
                            continue;

                        // Deduplicate
                        if (seenNumbers.contains(hn))
                            continue;
                        seenNumbers.insert(hn);

                        QPointF pt = VectorTile::decodePoint(feature.geometry);
                        double lon = (x + pt.x() / layer.extent) / n * 360.0 - 180.0;
                        // TMS: Y flipped within tile (same as RoadInfoService)
                        double yMerc = 1.0 - (y + 1.0 - pt.y() / layer.extent) / n;
                        double z = M_PI * (1.0 - 2.0 * yMerc);
                        double lat = std::atan(std::sinh(z)) * 180.0 / M_PI;

                        QVariantMap map;
                        map[QStringLiteral("housenumber")] = hn;
                        map[QStringLiteral("latitude")] = lat;
                        map[QStringLiteral("longitude")] = lon;
                        result.append(map);
                    }
                }
            }
        }

        qDebug() << "queryHouseNumbers: tiles=" << tilesFound << "addresses=" << totalAddresses
                 << "streetMatch=" << streetMatches << "cityMatch=" << cityMatches
                 << "results=" << result.size();

        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    // Sort naturally
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        QString ha = a.toMap()[QStringLiteral("housenumber")].toString();
        QString hb = b.toMap()[QStringLiteral("housenumber")].toString();
        int numA = 0, numB = 0, posA = 0, posB = 0;
        while (posA < ha.size() && ha[posA].isDigit()) { numA = numA * 10 + ha[posA].digitValue(); posA++; }
        while (posB < hb.size() && hb[posB].isDigit()) { numB = numB * 10 + hb[posB].digitValue(); posB++; }
        if (numA != numB) return numA < numB;
        return ha.mid(posA) < hb.mid(posB);
    });
    return result;
}

// ---------------------------------------------------------------------------
// Status management
// ---------------------------------------------------------------------------

void AddressDatabaseService::setStatus(Status s, const QString &message)
{
    bool statusChanged_ = (m_status != s);
    bool msgChanged = (m_statusMessage != message);
    m_status = s;
    m_statusMessage = message;
    if (statusChanged_) emit statusChanged();
    if (msgChanged) emit statusMessageChanged();
}

void AddressDatabaseService::onBuildProgress(double progress, int count)
{
    m_buildProgress = progress;
    emit buildProgressChanged();
    Q_UNUSED(count);
    emit addressCountChanged();
}

void AddressDatabaseService::cancelBuild()
{
    m_cancelRequested.store(true);
}

void AddressDatabaseService::onBuildFinished(bool success, const QString &error)
{
    m_cancelRequested.store(false);
    if (success) {
        buildTries();
        setStatus(Ready, QStringLiteral("Ready"));
        emit addressCountChanged();
    } else if (error == QLatin1String("Cancelled")) {
        setStatus(Idle, {});
    } else {
        setStatus(Error, error);
    }
}

// ---------------------------------------------------------------------------
// Build tries from m_allAddresses
// ---------------------------------------------------------------------------

struct TrieData {
    FlatTrie cityTrie;
    QHash<QString, std::shared_ptr<FlatTrie>> streetTries;
    QHash<QString, QHash<QString, AddressDatabaseService::StreetRecord>> streetData;
    int addressCount = 0;

    TrieData() = default;
    TrieData(TrieData &&) = default;
    TrieData &operator=(TrieData &&) = default;
    TrieData(const TrieData &) = delete;
    TrieData &operator=(const TrieData &) = delete;
};

// BuildResult must be copy-constructible to flow through QFuture<BuildResult>.
// TrieData is now move-only (unique_ptr FlatTries are not copyable) so we wrap
// it in a shared_ptr. The indirection is one pointer deep and happens exactly
// once per build.
struct BuildResult {
    bool success = false;
    bool fromCache = false;
    QString error;
    QString mapHash;
    QVector<AddressEntry> addresses; // temporary, used during build then discarded
    std::shared_ptr<TrieData> tries;
};

static TrieData buildTriesFromAddresses(QVector<AddressEntry> &addresses)
{
    // Post-process city names: merge "X-Y" into "X" when "X" exists as a standalone city
    {
        QSet<QString> baseCities;
        for (const auto &entry : addresses) {
            if (!entry.city.contains(QLatin1Char('-')))
                baseCities.insert(entry.city);
        }

        int merged = 0;
        for (auto &entry : addresses) {
            int idx = entry.city.indexOf(QLatin1Char('-'));
            if (idx > 0) {
                QString base = entry.city.left(idx);
                if (baseCities.contains(base)) {
                    entry.city = base;
                    merged++;
                }
            }
        }
        qDebug() << "AddressDatabase: merged" << merged << "hyphenated city entries";
    }

    TrieData data;
    data.addressCount = addresses.size();

    QSet<QString> seenCities;

    for (const auto &entry : addresses) {
        QString normCity = AddressDatabaseService::normalize(entry.city);
        QString normStreet = AddressDatabaseService::normalize(entry.street);

        if (!seenCities.contains(normCity)) {
            data.cityTrie.insert(normCity, entry.city);
            seenCities.insert(normCity);
        }

        auto &streetTriePtr = data.streetTries[normCity];
        if (!streetTriePtr)
            streetTriePtr = std::make_shared<FlatTrie>();
        streetTriePtr->insert(normStreet, entry.street);

        auto &streetRec = data.streetData[normCity][normStreet];
        if (streetRec.displayStreet.isEmpty())
            streetRec.displayStreet = entry.street;
        // Running centroids (overall + per-postcode)
        auto &c = streetRec.centroid;
        c.count++;
        c.lat += (entry.latitude - c.lat) / c.count;
        c.lng += (entry.longitude - c.lng) / c.count;
        if (!entry.postcode.isEmpty()) {
            auto &pc = streetRec.pcCentroidRef(entry.postcode);
            pc.count++;
            pc.lat += (entry.latitude - pc.lat) / pc.count;
            pc.lng += (entry.longitude - pc.lng) / pc.count;
        }
    }

    // Freeze all tries. FlatTrie::finalize() sorts children, computes subtree
    // counts, and releases the build-time scratch vectors.
    data.cityTrie.finalize();
    for (auto it = data.streetTries.begin(); it != data.streetTries.end(); ++it)
        it.value()->finalize();

    qDebug() << "AddressDatabase: built tries —"
             << data.streetData.size() << "cities,"
             << addresses.size() << "addresses";
    return data;
}

static void installTrieData(AddressDatabaseService *svc, TrieData &data);

void AddressDatabaseService::buildTries()
{
    // Not used directly — trie building happens on background thread
    Q_UNREACHABLE();
}

// ---------------------------------------------------------------------------
// Tile coordinate helpers
// ---------------------------------------------------------------------------

static int lonToTileX(double lon, int zoom)
{
    return static_cast<int>(std::floor((lon + 180.0) / 360.0 * (1 << zoom)));
}

static int latToTileYTMS(double lat, int zoom)
{
    double latRad = lat * M_PI / 180.0;
    double n = std::pow(2.0, zoom);
    double y = (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * n;
    // Convert slippy Y to TMS Y: MBTiles tile_row uses TMS (Y=0 at bottom)
    return static_cast<int>(n) - 1 - static_cast<int>(std::floor(y));
}

// ---------------------------------------------------------------------------
// Background build: extract addresses from mbtiles
// ---------------------------------------------------------------------------

static BuildResult buildFromTiles(AddressDatabaseService *service, const QString &mbtilesPath)
{
    BuildResult result;

    // Compute SHA-256 of the mbtiles file
    QFile mapFile(mbtilesPath);
    if (!mapFile.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Cannot open map file");
        return result;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&mapFile)) {
        result.error = QStringLiteral("Failed to hash map file");
        return result;
    }
    mapFile.close();
    result.mapHash = QString::fromLatin1(hash.result().toHex());

    // Open mbtiles database
    const QString connName = QStringLiteral("address_build");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(mbtilesPath);
        if (!db.open()) {
            result.error = QStringLiteral("Cannot open mbtiles database");
            QSqlDatabase::removeDatabase(connName);
            return result;
        }

        // Read bounds from metadata
        double minLng = 0, minLat = 0, maxLng = 0, maxLat = 0;
        {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT value FROM metadata WHERE name='bounds'")) && q.next()) {
                QStringList parts = q.value(0).toString().split(QLatin1Char(','));
                if (parts.size() == 4) {
                    minLng = parts[0].toDouble();
                    minLat = parts[1].toDouble();
                    maxLng = parts[2].toDouble();
                    maxLat = parts[3].toDouble();
                }
            }
        }

        if (minLng == 0 && maxLng == 0) {
            result.error = QStringLiteral("No bounds in mbtiles metadata");
            db.close();
            QSqlDatabase::removeDatabase(connName);
            return result;
        }

        // Compute tile ranges at zoom 14
        constexpr int zoom = 14;
        int minTileX = lonToTileX(minLng, zoom);
        int maxTileX = lonToTileX(maxLng, zoom);
        int minTileY = latToTileYTMS(maxLat, zoom);
        int maxTileY = latToTileYTMS(minLat, zoom);

        if (minTileY > maxTileY)
            std::swap(minTileY, maxTileY);

        int totalTiles = (maxTileX - minTileX + 1) * (maxTileY - minTileY + 1);
        int processed = 0;

        qDebug() << "AddressDatabase: building from" << totalTiles << "tiles at zoom" << zoom;

        constexpr double n = 16384.0; // 2^14

        QSqlQuery tileQuery(db);
        tileQuery.prepare(QStringLiteral(
            "SELECT tile_data FROM tiles WHERE zoom_level=14 AND tile_column=? AND tile_row=?"));

        for (int x = minTileX; x <= maxTileX; ++x) {
            if (service->isCancelled()) {
                db.close();
                QSqlDatabase::removeDatabase(connName);
                return BuildResult{false, false, QStringLiteral("Cancelled"), {}, {}};
            }

            for (int y = minTileY; y <= maxTileY; ++y) {
                processed++;

                if (processed % std::max(1, totalTiles / 20) == 0 || processed == totalTiles) {
                    double progress = static_cast<double>(processed) / totalTiles;
                    QMetaObject::invokeMethod(service, "onBuildProgress",
                        Qt::QueuedConnection,
                        Q_ARG(double, progress),
                        Q_ARG(int, result.addresses.size()));
                }

                tileQuery.bindValue(0, x);
                tileQuery.bindValue(1, y);
                if (!tileQuery.exec() || !tileQuery.next())
                    continue;

                QByteArray tileData = tileQuery.value(0).toByteArray();
                QByteArray decompressed = VectorTile::gunzip(tileData);
                if (decompressed.isEmpty())
                    continue;

                VectorTile::Tile tile = VectorTile::parse(decompressed);

                for (const auto &layer : tile.layers) {
                    if (layer.name != QLatin1String("addresses"))
                        continue;

                    for (const auto &feature : layer.features) {
                        if (feature.type != 1) // POINT
                            continue;

                        QString street = feature.properties.value(QStringLiteral("street"));
                        QString city = AddressDatabaseService::cleanCityName(feature.properties.value(QStringLiteral("city")));
                        QString housenumber = feature.properties.value(QStringLiteral("housenumber"));
                        QString postcode = feature.properties.value(QStringLiteral("postcode"));
                        QString name = feature.properties.value(QStringLiteral("name"));

                        // Use name as fallback for street
                        if (street.isEmpty())
                            street = name;

                        // Skip entries without both city and street, or bogus city names
                        if (city.isEmpty() || street.isEmpty())
                            continue;
                        if (city[0].isDigit())
                            continue;

                        QPointF pt = VectorTile::decodePoint(feature.geometry);

                        double lon = (x + pt.x() / layer.extent) / n * 360.0 - 180.0;
                        // TMS: Y flipped within tile (same as RoadInfoService)
                        double yMerc = 1.0 - (y + 1.0 - pt.y() / layer.extent) / n;
                        double z = M_PI * (1.0 - 2.0 * yMerc);
                        double lat = std::atan(std::sinh(z)) * 180.0 / M_PI;

                        result.addresses.append({city, street, housenumber, postcode, lat, lon});
                    }
                }
            }
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    result.success = true;
    qDebug() << "AddressDatabase: extracted" << result.addresses.size() << "addresses";

    // Build tries from addresses
    result.tries = std::make_shared<TrieData>(buildTriesFromAddresses(result.addresses));

    // Save compact cache (one entry per street with centroid, no house numbers)
    {
        QDir().mkpath(QFileInfo(AddressDatabaseService::CachePath).absolutePath());
        QJsonArray arr;
        const TrieData &tries = *result.tries;
        for (auto cityIt = tries.streetData.constBegin();
             cityIt != tries.streetData.constEnd(); ++cityIt) {
            // Find display city name from city trie
            QString displayCity = cityIt.key();
            uint32_t cityNode = tries.cityTrie.findNode(cityIt.key());
            if (tries.cityTrie.valid(cityNode) &&
                tries.cityTrie.hasDisplayName(cityNode)) {
                displayCity = tries.cityTrie.displayName(cityNode);
            }

            for (auto streetIt = cityIt.value().constBegin();
                 streetIt != cityIt.value().constEnd(); ++streetIt) {
                const auto &rec = streetIt.value();
                QJsonObject obj;
                obj[QStringLiteral("c")] = displayCity;
                obj[QStringLiteral("s")] = rec.displayStreet;
                obj[QStringLiteral("lat")] = rec.centroid.lat;
                obj[QStringLiteral("lng")] = rec.centroid.lng;
                if (rec.hasPostcodes()) {
                    QJsonArray pcs;
                    pcs.append(rec.firstPostcode);
                    for (const auto &e : rec.extraPcs)
                        pcs.append(e.first);
                    obj[QStringLiteral("p")] = pcs;

                    QJsonObject pcObj;
                    {
                        QJsonArray coords;
                        coords.append(rec.firstPcCentroid.lat);
                        coords.append(rec.firstPcCentroid.lng);
                        pcObj[rec.firstPostcode] = coords;
                    }
                    for (const auto &e : rec.extraPcs) {
                        QJsonArray coords;
                        coords.append(e.second.lat);
                        coords.append(e.second.lng);
                        pcObj[e.first] = coords;
                    }
                    obj[QStringLiteral("pc")] = pcObj;
                }
                arr.append(obj);
            }
        }
        QJsonObject root;
        root[QStringLiteral("version")] = 5;
        root[QStringLiteral("mapHash")] = result.mapHash;
        root[QStringLiteral("streets")] = arr;

        QString tmpPath = AddressDatabaseService::CachePath + QStringLiteral(".tmp");
        QFile file(tmpPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
            file.close();
            QFile::remove(AddressDatabaseService::CachePath);
            QFile::rename(tmpPath, AddressDatabaseService::CachePath);
            qDebug() << "AddressDatabase: saved cache with" << arr.size() << "streets";
        }
    }

    // Free raw addresses — no longer needed
    result.addresses.clear();
    result.addresses.squeeze();

    return result;
}

// ---------------------------------------------------------------------------
// Initialize: load from cache or build from tiles
// ---------------------------------------------------------------------------

void AddressDatabaseService::initialize()
{
    QString mbtilesPath = QFile::exists(QStringLiteral("map.mbtiles"))
        ? QStringLiteral("map.mbtiles")
        : MbtilesPath;

    if (!QFile::exists(mbtilesPath)) {
        setStatus(Error, QStringLiteral("Map file not found"));
        return;
    }

    m_cancelRequested.store(false);
    setStatus(Loading, QStringLiteral("Loading address database..."));

    auto *watcher = new QFutureWatcher<BuildResult>(this);

    connect(watcher, &QFutureWatcher<BuildResult>::finished, this, [this, watcher]() {
        BuildResult result = watcher->result();
        watcher->deleteLater();

        m_cancelRequested.store(false);
        if (!result.success) {
            if (result.error == QLatin1String("Cancelled"))
                setStatus(Idle, {});
            else
                setStatus(Error, result.error);
            return;
        }

        // Install pre-built tries (built on background thread). All members
        // are move-only value/unique_ptr types so destruction of the old
        // contents is implicit.
        if (result.tries) {
            m_cityTrie = std::move(result.tries->cityTrie);
            m_streetTries = std::move(result.tries->streetTries);
            m_streetData = std::move(result.tries->streetData);
            m_addressCount = result.tries->addressCount;
            result.tries.reset();
        }

        // Trie building churns through many temporary allocations (JSON parse,
        // QString interning, intermediate hash buckets) and leaves the sbrk
        // heap heavily fragmented — dump analysis showed ~230 MB of
        // never-reclaimed free space. Force glibc to return unused pages to
        // the kernel now that the steady-state data structure is installed.
#if defined(__GLIBC__)
        malloc_trim(0);
#endif

        setStatus(Ready, QStringLiteral("Ready"));
        emit addressCountChanged();
    });

    setStatus(Building, QStringLiteral("Building address database..."));

    watcher->setFuture(QtConcurrent::run([this]() -> BuildResult {
        // Quick hash check + cache load attempt
        QString mbtilesPath = QFile::exists(QStringLiteral("map.mbtiles"))
            ? QStringLiteral("map.mbtiles")
            : MbtilesPath;

        QFile mapFile(mbtilesPath);
        if (!mapFile.open(QIODevice::ReadOnly)) {
            return BuildResult{false, false, QStringLiteral("Cannot open map file"), {}, {}};
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&mapFile);
        mapFile.close();
        QString mapHash = QString::fromLatin1(hash.result().toHex());

        // Try loading from cache (v5: fixed TMS Y coordinate decoding)
        QFile cacheFile(CachePath);
        if (cacheFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(cacheFile.readAll());
            cacheFile.close();
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                if (root.value(QStringLiteral("version")).toInt() == 5 &&
                    root.value(QStringLiteral("mapHash")).toString() == mapHash) {
                    QJsonArray arr = root.value(QStringLiteral("streets")).toArray();

                    // Build tries directly from cached street records
                    TrieData data;
                    QSet<QString> seenCities;

                    for (const QJsonValue &v : arr) {
                        QJsonObject obj = v.toObject();
                        QString city = AddressDatabaseService::cleanCityName(obj[QStringLiteral("c")].toString());
                        QString street = obj[QStringLiteral("s")].toString();
                        double lat = obj[QStringLiteral("lat")].toDouble();
                        double lng = obj[QStringLiteral("lng")].toDouble();

                        QString normCity = AddressDatabaseService::normalize(city);
                        QString normStreet = AddressDatabaseService::normalize(street);

                        if (!seenCities.contains(normCity)) {
                            data.cityTrie.insert(normCity, city);
                            seenCities.insert(normCity);
                        }
                        auto &streetTriePtr = data.streetTries[normCity];
                        if (!streetTriePtr)
                            streetTriePtr = std::make_shared<FlatTrie>();
                        streetTriePtr->insert(normStreet, street);

                        auto &rec = data.streetData[normCity][normStreet];
                        if (rec.displayStreet.isEmpty())
                            rec.displayStreet = street;
                        rec.centroid.lat = lat;
                        rec.centroid.lng = lng;
                        rec.centroid.count = 1;

                        // Load per-postcode centroids. `pc` (object) is the
                        // source of truth for which postcodes exist; `p` (array)
                        // is redundant bookkeeping kept only for older cache
                        // readers.
                        QJsonObject pcObj = obj[QStringLiteral("pc")].toObject();
                        for (auto pcIt = pcObj.constBegin(); pcIt != pcObj.constEnd(); ++pcIt) {
                            QJsonArray coords = pcIt.value().toArray();
                            if (coords.size() >= 2) {
                                auto &pcd = rec.pcCentroidRef(pcIt.key());
                                pcd.lat = coords[0].toDouble();
                                pcd.lng = coords[1].toDouble();
                                pcd.count = 1;
                            }
                        }
                        // Fall back to `p` if `pc` is missing but `p` is present
                        // (older caches may have had postcodes without per-pc
                        // centroids; use the overall centroid for each).
                        if (pcObj.isEmpty()) {
                            QJsonValue pVal = obj[QStringLiteral("p")];
                            auto addPc = [&](const QString &pc) {
                                if (pc.isEmpty()) return;
                                auto &pcd = rec.pcCentroidRef(pc);
                                pcd.lat = lat;
                                pcd.lng = lng;
                                pcd.count = 1;
                            };
                            if (pVal.isArray()) {
                                for (const QJsonValue &pc : pVal.toArray())
                                    addPc(pc.toString());
                            } else if (pVal.isString()) {
                                addPc(pVal.toString());
                            }
                        }
                        data.addressCount++;
                    }

                    // Freeze all tries. finalize() sorts children, computes
                    // subtree counts, and releases build-time scratch.
                    data.cityTrie.finalize();
                    for (auto it = data.streetTries.begin(); it != data.streetTries.end(); ++it)
                        it.value()->finalize();

                    qDebug() << "AddressDatabase: loaded" << arr.size() << "streets from cache";
                    return BuildResult{true, true, {}, mapHash, {},
                                       std::make_shared<TrieData>(std::move(data))};
                }
            }
        }

        // Cache is stale or missing — build from tiles
        return buildFromTiles(this, mbtilesPath);
    }));
}

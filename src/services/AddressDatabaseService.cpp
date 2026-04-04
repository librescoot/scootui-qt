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

const QString AddressDatabaseService::MbtilesPath = QStringLiteral("/data/maps/map.mbtiles");
const QString AddressDatabaseService::CachePath = QStringLiteral("/data/scootui/address_database.json");

AddressDatabaseService::AddressDatabaseService(QObject *parent)
    : QObject(parent)
{
}

AddressDatabaseService::~AddressDatabaseService()
{
    delete m_cityTrieRoot;
    m_cityTrieRoot = nullptr;
    qDeleteAll(m_streetTries);
}


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
    result.reserve(name.size() + 4); // may grow slightly from ä→ae etc.

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
            } else if (name[i] == QLatin1Char(' ') || name[i] == QLatin1Char('-') ||
                       name[i] == QLatin1Char('.')) {
                result += name[i];
            }
            // drop other special characters
            break;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Trie operations
// ---------------------------------------------------------------------------

void AddressDatabaseService::insertIntoTrie(TrieNode *root, const QString &normalizedName,
                                             const QString &displayName)
{
    TrieNode *node = root;
    for (const QChar &ch : normalizedName) {
        auto it = node->children.find(ch);
        if (it == node->children.end())
            it = node->children.insert(ch, new TrieNode());
        node = it.value();
    }
    if (node->displayName.isEmpty())
        node->displayName = displayName;
}

const TrieNode *AddressDatabaseService::findNode(const TrieNode *root, const QString &prefix) const
{
    const TrieNode *node = root;
    for (const QChar &ch : prefix) {
        auto it = node->children.constFind(ch);
        if (it == node->children.constEnd())
            return nullptr;
        node = it.value();
    }
    return node;
}

void AddressDatabaseService::collectDisplayNames(const TrieNode *node, QStringList &out) const
{
    if (!node->displayName.isEmpty())
        out.append(node->displayName);
    for (auto it = node->children.constBegin(); it != node->children.constEnd(); ++it)
        collectDisplayNames(it.value(), out);
}

// ---------------------------------------------------------------------------
// City trie queries
// ---------------------------------------------------------------------------

QStringList AddressDatabaseService::getValidCityChars(const QString &prefix) const
{
    if (m_status != Ready || !m_cityTrieRoot)
        return {};
    const TrieNode *node = findNode(m_cityTrieRoot, normalize(prefix));
    if (!node)
        return {};
    QStringList chars;
    chars.reserve(node->children.size());
    for (auto it = node->children.constBegin(); it != node->children.constEnd(); ++it) {
        chars.append(QString(it.key()));
    }
    chars.sort();
    return chars;
}

int AddressDatabaseService::getCityCount(const QString &prefix) const
{
    if (m_status != Ready || !m_cityTrieRoot)
        return 0;
    const TrieNode *node = findNode(m_cityTrieRoot, normalize(prefix));
    if (!node)
        return 0;
    return node->subtreeUniqueCount;
}

QStringList AddressDatabaseService::getMatchingCities(const QString &prefix) const
{
    if (m_status != Ready || !m_cityTrieRoot)
        return {};
    const TrieNode *node = findNode(m_cityTrieRoot, normalize(prefix));
    if (!node)
        return {};
    QStringList result;
    collectDisplayNames(node, result);
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
    if (trieIt == m_streetTries.constEnd())
        return {};
    const TrieNode *node = findNode(trieIt.value(), normalize(prefix));
    if (!node)
        return {};
    QStringList chars;
    chars.reserve(node->children.size());
    for (auto it = node->children.constBegin(); it != node->children.constEnd(); ++it) {
        chars.append(QString(it.key()));
    }
    chars.sort();
    return chars;
}

int AddressDatabaseService::getStreetCount(const QString &city, const QString &prefix) const
{
    if (m_status != Ready)
        return 0;
    QString normCity = normalize(city);
    auto trieIt = m_streetTries.constFind(normCity);
    if (trieIt == m_streetTries.constEnd())
        return 0;
    const TrieNode *node = findNode(trieIt.value(), normalize(prefix));
    if (!node)
        return 0;
    return node->subtreeUniqueCount;
}

QVariantList AddressDatabaseService::getMatchingStreets(const QString &city, const QString &prefix) const
{
    if (m_status != Ready)
        return {};
    QString normCity = normalize(city);
    auto trieIt = m_streetTries.constFind(normCity);
    if (trieIt == m_streetTries.constEnd())
        return {};
    const TrieNode *node = findNode(trieIt.value(), normalize(prefix));
    if (!node)
        return {};

    // Collect display names from trie terminals
    QStringList streetNames;
    collectDisplayNames(node, streetNames);

    // Look up postcodes from m_streetData to detect disambiguation
    auto cityDataIt = m_streetData.constFind(normCity);
    QHash<QString, QSet<QString>> streetPostcodes;
    if (cityDataIt != m_streetData.constEnd()) {
        for (const QString &name : streetNames) {
            QString normStreet = normalize(name);
            auto streetIt = cityDataIt.value().constFind(normStreet);
            if (streetIt != cityDataIt.value().constEnd()) {
                for (const auto &h : streetIt.value().houses)
                    streetPostcodes[name].insert(h.postcode);
            }
        }
    }

    // Build result — disambiguate streets with multiple postcodes
    QList<QPair<QString, QString>> pairs;
    for (const QString &street : streetNames) {
        const auto &pcs = streetPostcodes[street];
        if (pcs.size() > 1) {
            for (const QString &pc : pcs)
                pairs.append({street, pc});
        } else {
            pairs.append({street, pcs.isEmpty() ? QString() : *pcs.constBegin()});
        }
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

QVariantList AddressDatabaseService::getHouseNumbers(const QString &city, const QString &street, const QString &postcode) const
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

    QVariantList result;
    for (const auto &h : streetIt.value().houses) {
        if (!postcode.isEmpty() && h.postcode != postcode)
            continue;
        if (h.housenumber.isEmpty())
            continue;
        QVariantMap map;
        map[QStringLiteral("housenumber")] = h.housenumber;
        map[QStringLiteral("latitude")] = h.latitude;
        map[QStringLiteral("longitude")] = h.longitude;
        result.append(map);
    }

    // Sort house numbers naturally (1, 2, 3, 10, 10a, 11, ...)
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

    double sumLat = 0, sumLng = 0;
    int count = 0;
    for (const auto &h : streetIt.value().houses) {
        sumLat += h.latitude;
        sumLng += h.longitude;
        count++;
    }

    QVariantMap result;
    if (count > 0) {
        result[QStringLiteral("latitude")] = sumLat / count;
        result[QStringLiteral("longitude")] = sumLng / count;
    }
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
    TrieNode *cityTrieRoot = nullptr;
    QHash<QString, TrieNode *> streetTries;
    QHash<QString, QHash<QString, AddressDatabaseService::StreetRecord>> streetData;
    int addressCount = 0;
};

struct BuildResult {
    bool success = false;
    bool fromCache = false;
    QString error;
    QString mapHash;
    QVector<AddressEntry> addresses; // temporary, used during build then discarded
    TrieData tries;
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
    data.cityTrieRoot = new TrieNode();
    data.addressCount = addresses.size();

    QSet<QString> seenCities; // track which normalized cities have been inserted into trie

    for (const auto &entry : addresses) {
        QString normCity = AddressDatabaseService::normalize(entry.city);
        QString normStreet = AddressDatabaseService::normalize(entry.street);

        // Insert city into trie (only once per unique normalized name)
        if (!seenCities.contains(normCity)) {
            AddressDatabaseService::insertIntoTrie(data.cityTrieRoot, normCity, entry.city);
            seenCities.insert(normCity);
        }

        // Create street trie for this city if needed
        if (!data.streetTries.contains(normCity))
            data.streetTries[normCity] = new TrieNode();

        // Insert street into trie (insertIntoTrie is idempotent for display name)
        AddressDatabaseService::insertIntoTrie(data.streetTries[normCity], normStreet, entry.street);

        // Store house entry in grouped data
        auto &streetRec = data.streetData[normCity][normStreet];
        if (streetRec.displayStreet.isEmpty())
            streetRec.displayStreet = entry.street;
        streetRec.houses.append({entry.housenumber, entry.postcode, entry.latitude, entry.longitude});
    }

    // Precompute subtree unique counts (bottom-up)
    std::function<int(TrieNode *)> computeCounts = [&](TrieNode *node) -> int {
        int count = node->displayName.isEmpty() ? 0 : 1;
        for (auto it = node->children.begin(); it != node->children.end(); ++it)
            count += computeCounts(it.value());
        node->subtreeUniqueCount = count;
        return count;
    };
    computeCounts(data.cityTrieRoot);
    for (auto it = data.streetTries.begin(); it != data.streetTries.end(); ++it)
        computeCounts(it.value());

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
    int tmsY = static_cast<int>(std::floor(n - 1 - y)) + 1;
    return tmsY;
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
                        double yMerc = 1.0 - (y + pt.y() / layer.extent) / n;
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
    result.tries = buildTriesFromAddresses(result.addresses);
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

        // Install pre-built tries (built on background thread)
        delete m_cityTrieRoot;
        qDeleteAll(m_streetTries);
        m_cityTrieRoot = result.tries.cityTrieRoot;
        result.tries.cityTrieRoot = nullptr;
        m_streetTries = std::move(result.tries.streetTries);
        m_streetData = std::move(result.tries.streetData);
        m_addressCount = result.tries.addressCount;

        // Save cache if we built from tiles (use the raw addresses before they're discarded)
        if (!result.fromCache) {
            QString mapHash = result.mapHash;
            QVector<AddressEntry> addrs = std::move(result.addresses);
            (void)QtConcurrent::run([mapHash, addrs]() {
                QDir().mkpath(QFileInfo(AddressDatabaseService::CachePath).absolutePath());

                QJsonArray arr;
                for (const auto &addr : addrs) {
                    QJsonObject obj;
                    obj[QStringLiteral("c")] = addr.city;
                    obj[QStringLiteral("s")] = addr.street;
                    if (!addr.housenumber.isEmpty())
                        obj[QStringLiteral("h")] = addr.housenumber;
                    if (!addr.postcode.isEmpty())
                        obj[QStringLiteral("p")] = addr.postcode;
                    obj[QStringLiteral("lat")] = addr.latitude;
                    obj[QStringLiteral("lng")] = addr.longitude;
                    arr.append(obj);
                }

                QJsonObject root;
                root[QStringLiteral("version")] = 3;
                root[QStringLiteral("mapHash")] = mapHash;
                root[QStringLiteral("addresses")] = arr;

                QString tmpPath = AddressDatabaseService::CachePath + QStringLiteral(".tmp");
                QFile file(tmpPath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
                    file.close();
                    QFile::remove(AddressDatabaseService::CachePath);
                    QFile::rename(tmpPath, AddressDatabaseService::CachePath);
                    qDebug() << "AddressDatabase: saved cache with" << addrs.size() << "addresses";
                }
            });
        }

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

        // Try loading from cache
        QFile cacheFile(CachePath);
        if (cacheFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(cacheFile.readAll());
            cacheFile.close();
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                if (root.value(QStringLiteral("version")).toInt() == 3 &&
                    root.value(QStringLiteral("mapHash")).toString() == mapHash) {
                    QJsonArray arr = root.value(QStringLiteral("addresses")).toArray();
                    QVector<AddressEntry> addresses;
                    addresses.reserve(arr.size());
                    for (const QJsonValue &v : arr) {
                        QJsonObject obj = v.toObject();
                        addresses.append({
                            AddressDatabaseService::cleanCityName(obj[QStringLiteral("c")].toString()),
                            obj[QStringLiteral("s")].toString(),
                            obj[QStringLiteral("h")].toString(),
                            obj[QStringLiteral("p")].toString(),
                            obj[QStringLiteral("lat")].toDouble(),
                            obj[QStringLiteral("lng")].toDouble()
                        });
                    }
                    qDebug() << "AddressDatabase: loaded" << addresses.size() << "addresses from cache";
                    TrieData tries = buildTriesFromAddresses(addresses);
                    return BuildResult{true, true, {}, mapHash, std::move(addresses), std::move(tries)};
                }
            }
        }

        // Cache is stale or missing — build from tiles
        return buildFromTiles(this, mbtilesPath);
    }));
}

#include "AddressDatabaseService.h"
#include "VectorTileDecoder.h"

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QtMath>

const QString AddressDatabaseService::MbtilesPath = QStringLiteral("/data/maps/map.mbtiles");
const QString AddressDatabaseService::CachePath = QStringLiteral("/data/scootui/address_database.json");
const QString AddressDatabaseService::Base32Chars = QStringLiteral("0123456789ABCDEFGHJKMNPQRSTVWXYZ");

AddressDatabaseService::AddressDatabaseService(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Base32 decoding
// ---------------------------------------------------------------------------

int AddressDatabaseService::fromBase32(const QString &code)
{
    int result = 0;
    for (int i = 0; i < code.length(); ++i) {
        int idx = Base32Chars.indexOf(code[i].toUpper());
        if (idx < 0)
            return -1;
        result = result * 32 + idx;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Code lookup
// ---------------------------------------------------------------------------

QVariantMap AddressDatabaseService::lookupCode(const QString &code) const
{
    QVariantMap result;
    if (m_status != Ready || code.length() != 4) {
        result[QStringLiteral("valid")] = false;
        return result;
    }

    int index = fromBase32(code);
    if (index < 0 || index >= m_addresses.size()) {
        result[QStringLiteral("valid")] = false;
        return result;
    }

    const auto &addr = m_addresses[index];
    result[QStringLiteral("valid")] = true;
    result[QStringLiteral("latitude")] = addr.first;
    result[QStringLiteral("longitude")] = addr.second;
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

// ---------------------------------------------------------------------------
// Cache load/save (JSON, matching Flutter v2 format)
// ---------------------------------------------------------------------------

bool AddressDatabaseService::loadCache(const QString &mapHash)
{
    QFile file(CachePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
        return false;

    QJsonObject root = doc.object();
    if (root.value(QStringLiteral("mapHash")).toString() != mapHash)
        return false;

    QJsonArray arr = root.value(QStringLiteral("addresses")).toArray();
    m_addresses.clear();
    m_addresses.reserve(arr.size());

    for (const QJsonValue &v : arr) {
        QJsonArray coord = v.toArray();
        if (coord.size() >= 2) {
            m_addresses.append({coord[0].toDouble(), coord[1].toDouble()});
        }
    }

    qDebug() << "AddressDatabase: loaded" << m_addresses.size() << "addresses from cache";
    return true;
}

void AddressDatabaseService::saveCache(const QString &mapHash)
{
    QDir().mkpath(QFileInfo(CachePath).absolutePath());

    QJsonArray arr;
    for (const auto &addr : m_addresses) {
        arr.append(QJsonArray{addr.first, addr.second});
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 2;
    root[QStringLiteral("mapHash")] = mapHash;
    root[QStringLiteral("addresses")] = arr;

    // Write atomically via temp file
    QString tmpPath = CachePath + QStringLiteral(".tmp");
    QFile file(tmpPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
        QFile::remove(CachePath);
        QFile::rename(tmpPath, CachePath);
        qDebug() << "AddressDatabase: saved cache with" << m_addresses.size() << "addresses";
    }
}

// ---------------------------------------------------------------------------
// Progress callback from background thread
// ---------------------------------------------------------------------------

void AddressDatabaseService::onBuildProgress(double progress, int count)
{
    m_buildProgress = progress;
    emit buildProgressChanged();

    // Update address count for display
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
        setStatus(Ready, QStringLiteral("Ready"));
        emit addressCountChanged();
    } else if (error == QLatin1String("Cancelled")) {
        setStatus(Idle, {});
    } else {
        setStatus(Error, error);
    }
}

// ---------------------------------------------------------------------------
// Tile coordinate helpers (matching Flutter implementation)
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

struct BuildResult {
    bool success = false;
    QString error;
    QString mapHash;
    QVector<QPair<double, double>> addresses;
};

static BuildResult buildFromTiles(AddressDatabaseService *service)
{
    BuildResult result;
    const QString mbtilesPath = AddressDatabaseService::MbtilesPath;

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
        int minTileY = latToTileYTMS(maxLat, zoom); // top lat → min TMS y
        int maxTileY = latToTileYTMS(minLat, zoom); // bottom lat → max TMS y

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
                return BuildResult{false, QStringLiteral("Cancelled"), {}, {}};
            }

            for (int y = minTileY; y <= maxTileY; ++y) {
                processed++;

                // Report progress every ~5%
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

                        QPointF pt = VectorTile::decodePoint(feature.geometry);

                        // Convert tile coordinates to geographic coordinates
                        // (matching Flutter's conversion exactly)
                        double lon = (x + pt.x() / layer.extent) / n * 360.0 - 180.0;
                        double yMerc = 1.0 - (y + pt.y() / layer.extent) / n;
                        double z = M_PI * (1.0 - 2.0 * yMerc);
                        double lat = std::atan(std::sinh(z)) * 180.0 / M_PI;

                        result.addresses.append({lat, lon});
                    }
                }
            }
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    result.success = true;
    qDebug() << "AddressDatabase: extracted" << result.addresses.size() << "addresses";
    return result;
}

// ---------------------------------------------------------------------------
// Initialize: load from cache or build from tiles
// ---------------------------------------------------------------------------

void AddressDatabaseService::initialize()
{
    if (!QFile::exists(MbtilesPath)) {
        setStatus(Error, QStringLiteral("Map file not found"));
        return;
    }

    m_cancelRequested.store(false);
    setStatus(Loading, QStringLiteral("Loading address database..."));

    // Try loading from cache first — need map hash to validate
    // We do the full work in the background to avoid blocking
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

        // Try loading cached data first
        if (loadCache(result.mapHash)) {
            setStatus(Ready, QStringLiteral("Ready"));
            emit addressCountChanged();
            return;
        }

        // Cache miss or stale — use the built data
        m_addresses = std::move(result.addresses);
        saveCache(result.mapHash);
        setStatus(Ready, QStringLiteral("Ready"));
        emit addressCountChanged();
    });

    setStatus(Building, QStringLiteral("Building address database..."));

    // Compute hash first, try cache, build only if needed
    watcher->setFuture(QtConcurrent::run([this]() -> BuildResult {
        // Quick hash check + cache load attempt
        QFile mapFile(MbtilesPath);
        if (!mapFile.open(QIODevice::ReadOnly)) {
            return BuildResult{false, QStringLiteral("Cannot open map file"), {}, {}};
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&mapFile);
        mapFile.close();
        QString mapHash = QString::fromLatin1(hash.result().toHex());

        // Check if cache is valid (read-only check from worker thread)
        QFile cacheFile(CachePath);
        if (cacheFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(cacheFile.readAll());
            cacheFile.close();
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                if (root.value(QStringLiteral("mapHash")).toString() == mapHash) {
                    // Cache is valid — return success with empty addresses
                    // The main thread will load from cache
                    return BuildResult{true, {}, mapHash, {}};
                }
            }
        }

        // Cache is stale or missing — build from tiles
        return buildFromTiles(this);
    }));
}

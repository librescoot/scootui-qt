#pragma once

#include "FlatTrie.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <atomic>
#include <memory>

struct AddressEntry {
    QString city;
    QString street;
    QString housenumber;
    QString postcode;
    double latitude;
    double longitude;
};

class AddressDatabaseService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(double buildProgress READ buildProgress NOTIFY buildProgressChanged)
    Q_PROPERTY(int addressCount READ addressCount NOTIFY addressCountChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    enum Status { Idle, Loading, Building, Ready, Error };
    Q_ENUM(Status)

    explicit AddressDatabaseService(QObject *parent = nullptr);
    ~AddressDatabaseService();

    int status() const { return m_status; }
    double buildProgress() const { return m_buildProgress; }
    int addressCount() const { return m_addressCount; }
    QString statusMessage() const { return m_statusMessage; }

    // --- City trie queries ---
    Q_INVOKABLE QStringList getValidCityChars(const QString &prefix) const;
    Q_INVOKABLE int getCityCount(const QString &prefix) const;
    Q_INVOKABLE QStringList getMatchingCities(const QString &prefix) const;

    // --- Street trie queries (within a city) ---
    Q_INVOKABLE QStringList getValidStreetChars(const QString &city, const QString &prefix) const;
    Q_INVOKABLE int getStreetCount(const QString &city, const QString &prefix) const;
    Q_INVOKABLE QVariantList getMatchingStreets(const QString &city, const QString &prefix) const;

    // --- House number / coordinate queries ---
    Q_INVOKABLE void queryHouseNumbers(const QString &city, const QString &street, const QString &postcode);
    Q_INVOKABLE QVariantMap getStreetCoordinates(const QString &city, const QString &street) const;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void cancelBuild();
    bool isCancelled() const { return m_cancelRequested.load(); }

signals:
    void statusChanged();
    void buildProgressChanged();
    void addressCountChanged();
    void statusMessageChanged();
    void houseNumbersReady(const QVariantList &houses);

public slots:
    void onBuildProgress(double progress, int count);
    void onBuildFinished(bool success, const QString &error);

private:
    void setStatus(Status s, const QString &message = {});
    void buildTries();

public:
    static QString normalize(const QString &name);
    static QString cleanCityName(const QString &raw);

private:

    Status m_status = Idle;
    double m_buildProgress = 0;
    QString m_statusMessage;
    std::atomic<bool> m_cancelRequested{false};
    int m_addressCount = 0;

    // City trie: normalized city name → terminal nodes with display name
    FlatTrie m_cityTrie;

    // Per-city street tries: normalized city name → street trie.
    // shared_ptr rather than unique_ptr because Qt6 QHash bakes a copy of the
    // value type into its detach() path at template-instantiation time, even
    // when the runtime path doesn't actually copy. unique_ptr won't compile;
    // shared_ptr copies (the rare times QHash detaches) are cheap ref-count
    // bumps. The indirection is one pointer per city (~400 cities max).
    QHash<QString, std::shared_ptr<FlatTrie>> m_streetTries;

public:
    struct CentroidData {
        double lat = 0;
        double lng = 0;
        int count = 0;
    };
    // Per-street record. Qt6 QHash/QSet pre-allocate a full bucket table on
    // first insert (~5 KB each), so a naive QHash<postcode, centroid> burns
    // ~10 KB per street for what is almost always a single postcode. With
    // ~50k streets that adds up to ~500 MB of mostly-empty buckets. Instead
    // we inline the common 1-postcode case and spill to a QVector for the
    // rare multi-postcode streets.
    struct StreetRecord {
        QString displayStreet;
        CentroidData centroid;                // overall centroid across all postcodes
        QString firstPostcode;                // empty = no postcode data at all
        CentroidData firstPcCentroid;         // valid iff firstPostcode non-empty
        QVector<QPair<QString, CentroidData>> extraPcs; // empty in ~99% of cases

        int postcodeCount() const {
            return firstPostcode.isEmpty() ? 0 : 1 + int(extraPcs.size());
        }
        bool hasPostcodes() const { return !firstPostcode.isEmpty(); }

        // Look up an existing per-postcode centroid (read-only).
        const CentroidData *findPcCentroid(const QString &pc) const {
            if (firstPostcode == pc)
                return &firstPcCentroid;
            for (const auto &e : extraPcs)
                if (e.first == pc)
                    return &e.second;
            return nullptr;
        }

        // Insert or return a mutable reference to the centroid for a postcode.
        CentroidData &pcCentroidRef(const QString &pc) {
            if (firstPostcode.isEmpty()) {
                firstPostcode = pc;
                return firstPcCentroid;
            }
            if (firstPostcode == pc)
                return firstPcCentroid;
            for (auto &e : extraPcs)
                if (e.first == pc)
                    return e.second;
            extraPcs.append({pc, CentroidData{}});
            return extraPcs.last().second;
        }
    };

private:
    QHash<QString, QHash<QString, StreetRecord>> m_streetData;

    // On-demand house number lookup from mbtiles
    QVariantList queryHouseNumbersFromTiles(const QString &city, const QString &street,
                                            const QString &postcode, double nearLat, double nearLng) const;

public:
    static const QString MbtilesPath;
    static const QString CachePath;
};

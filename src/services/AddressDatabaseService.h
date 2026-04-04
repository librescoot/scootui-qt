#pragma once

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <atomic>

struct AddressEntry {
    QString city;
    QString street;
    QString housenumber;
    QString postcode;
    double latitude;
    double longitude;
};

struct TrieNode {
    QHash<QChar, TrieNode *> children;
    QString displayName;        // non-empty at terminal nodes (original display name)
    int subtreeUniqueCount = 0; // number of distinct terminal nodes in subtree

    ~TrieNode() { qDeleteAll(children); }
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
    Q_INVOKABLE QVariantList getHouseNumbers(const QString &city, const QString &street, const QString &postcode) const;
    Q_INVOKABLE QVariantMap getStreetCoordinates(const QString &city, const QString &street) const;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void cancelBuild();
    bool isCancelled() const { return m_cancelRequested.load(); }

signals:
    void statusChanged();
    void buildProgressChanged();
    void addressCountChanged();
    void statusMessageChanged();

public slots:
    void onBuildProgress(double progress, int count);
    void onBuildFinished(bool success, const QString &error);

private:
    void setStatus(Status s, const QString &message = {});
    void buildTries();

    const TrieNode *findNode(const TrieNode *root, const QString &prefix) const;
    void collectDisplayNames(const TrieNode *node, QStringList &out) const;

public:
    static QString normalize(const QString &name);
    static QString cleanCityName(const QString &raw);
    static void insertIntoTrie(TrieNode *root, const QString &normalizedName,
                               const QString &displayName);

private:

    Status m_status = Idle;
    double m_buildProgress = 0;
    QString m_statusMessage;
    std::atomic<bool> m_cancelRequested{false};
    int m_addressCount = 0;

    // City trie: normalized city name → terminal nodes with display name
    TrieNode *m_cityTrieRoot = nullptr;

    // Per-city street tries: normalized city name → street trie root
    QHash<QString, TrieNode *> m_streetTries;

public:
    // Grouped address data: normCity → normStreet → list of {housenumber, postcode, lat, lng}
    struct HouseEntry {
        QString housenumber;
        QString postcode;
        double latitude;
        double longitude;
    };
    struct StreetRecord {
        QString displayStreet;
        QVector<HouseEntry> houses;
    };

private:
    QHash<QString, QHash<QString, StreetRecord>> m_streetData;

public:
    static const QString MbtilesPath;
    static const QString CachePath;
};

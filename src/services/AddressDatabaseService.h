#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVector>
#include <QPair>

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

    int status() const { return m_status; }
    double buildProgress() const { return m_buildProgress; }
    int addressCount() const { return m_addresses.size(); }
    QString statusMessage() const { return m_statusMessage; }

    // Look up a base32 code → {valid, latitude, longitude}
    Q_INVOKABLE QVariantMap lookupCode(const QString &code) const;

    // Start loading/building the database
    void initialize();

signals:
    void statusChanged();
    void buildProgressChanged();
    void addressCountChanged();
    void statusMessageChanged();

public slots:
    // Called from background thread via queued connection
    void onBuildProgress(double progress, int count);
    void onBuildFinished(bool success, const QString &error);

private:
    static int fromBase32(const QString &code);
    void setStatus(Status s, const QString &message = {});
    bool loadCache(const QString &mapHash);
    void saveCache(const QString &mapHash);

    Status m_status = Idle;
    double m_buildProgress = 0;
    QString m_statusMessage;

    // Ordered list: index = base32-decoded code value
    QVector<QPair<double, double>> m_addresses; // (lat, lng)

public:
    static const QString MbtilesPath;
private:
    static const QString CachePath;
    static const QString Base32Chars;
};

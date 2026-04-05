#pragma once

#include "SyncableStore.h"

class OtaStore : public SyncableStore
{
    Q_OBJECT
    Q_PROPERTY(QString dbcStatus READ dbcStatus NOTIFY dbcStatusChanged)
    Q_PROPERTY(QString dbcUpdateVersion READ dbcUpdateVersion NOTIFY dbcUpdateVersionChanged)
    Q_PROPERTY(QString dbcUpdateMethod READ dbcUpdateMethod NOTIFY dbcUpdateMethodChanged)
    Q_PROPERTY(QString dbcError READ dbcError NOTIFY dbcErrorChanged)
    Q_PROPERTY(QString dbcErrorMessage READ dbcErrorMessage NOTIFY dbcErrorMessageChanged)
    Q_PROPERTY(int dbcDownloadProgress READ dbcDownloadProgress NOTIFY dbcDownloadProgressChanged)
    Q_PROPERTY(int dbcInstallProgress READ dbcInstallProgress NOTIFY dbcInstallProgressChanged)
    Q_PROPERTY(QString mdbStatus READ mdbStatus NOTIFY mdbStatusChanged)
    Q_PROPERTY(QString mdbUpdateVersion READ mdbUpdateVersion NOTIFY mdbUpdateVersionChanged)
    Q_PROPERTY(QString mdbUpdateMethod READ mdbUpdateMethod NOTIFY mdbUpdateMethodChanged)
    Q_PROPERTY(QString mdbError READ mdbError NOTIFY mdbErrorChanged)
    Q_PROPERTY(QString mdbErrorMessage READ mdbErrorMessage NOTIFY mdbErrorMessageChanged)
    Q_PROPERTY(int mdbDownloadProgress READ mdbDownloadProgress NOTIFY mdbDownloadProgressChanged)
    Q_PROPERTY(int mdbInstallProgress READ mdbInstallProgress NOTIFY mdbInstallProgressChanged)
    Q_PROPERTY(bool isActive READ isActive NOTIFY isActiveChanged)

public:
    explicit OtaStore(MdbRepository *repo, QObject *parent = nullptr);

    QString dbcStatus() const { return m_dbcStatus; }
    QString dbcUpdateVersion() const { return m_dbcUpdateVersion; }
    QString dbcUpdateMethod() const { return m_dbcUpdateMethod; }
    QString dbcError() const { return m_dbcError; }
    QString dbcErrorMessage() const { return m_dbcErrorMessage; }
    int dbcDownloadProgress() const { return m_dbcDownloadProgress; }
    int dbcInstallProgress() const { return m_dbcInstallProgress; }
    QString mdbStatus() const { return m_mdbStatus; }
    QString mdbUpdateVersion() const { return m_mdbUpdateVersion; }
    QString mdbUpdateMethod() const { return m_mdbUpdateMethod; }
    QString mdbError() const { return m_mdbError; }
    QString mdbErrorMessage() const { return m_mdbErrorMessage; }
    int mdbDownloadProgress() const { return m_mdbDownloadProgress; }
    int mdbInstallProgress() const { return m_mdbInstallProgress; }
    bool isActive() const;

    Q_INVOKABLE void setBacklightOff(bool off);

signals:
    void dbcStatusChanged();
    void dbcUpdateVersionChanged();
    void dbcUpdateMethodChanged();
    void dbcErrorChanged();
    void dbcErrorMessageChanged();
    void dbcDownloadProgressChanged();
    void dbcInstallProgressChanged();
    void mdbStatusChanged();
    void mdbUpdateVersionChanged();
    void mdbUpdateMethodChanged();
    void mdbErrorChanged();
    void mdbErrorMessageChanged();
    void mdbDownloadProgressChanged();
    void mdbInstallProgressChanged();
    void isActiveChanged();

protected:
    SyncSettings syncSettings() const override;
    void applyFieldUpdate(const QString &variable, const QString &value) override;

private:
    QString m_dbcStatus = QStringLiteral("idle");
    QString m_dbcUpdateVersion;
    QString m_dbcUpdateMethod;
    QString m_dbcError;
    QString m_dbcErrorMessage;
    int m_dbcDownloadProgress = 0;
    int m_dbcInstallProgress = 0;
    QString m_mdbStatus = QStringLiteral("idle");
    QString m_mdbUpdateVersion;
    QString m_mdbUpdateMethod;
    QString m_mdbError;
    QString m_mdbErrorMessage;
    int m_mdbDownloadProgress = 0;
    int m_mdbInstallProgress = 0;
};

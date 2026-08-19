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
    // Channel preview, one set per component. update-service publishes these
    // in answer to a preview-channel: command; they say what a switch to that
    // channel would fetch and are unrelated to any update in flight.
    Q_PROPERTY(QString dbcPreviewChannel READ dbcPreviewChannel NOTIFY dbcPreviewChanged)
    Q_PROPERTY(QString dbcPreviewStatus READ dbcPreviewStatus NOTIFY dbcPreviewChanged)
    Q_PROPERTY(QString dbcPreviewVersion READ dbcPreviewVersion NOTIFY dbcPreviewChanged)
    Q_PROPERTY(qint64 dbcPreviewSize READ dbcPreviewSize NOTIFY dbcPreviewChanged)
    Q_PROPERTY(QString mdbPreviewChannel READ mdbPreviewChannel NOTIFY mdbPreviewChanged)
    Q_PROPERTY(QString mdbPreviewStatus READ mdbPreviewStatus NOTIFY mdbPreviewChanged)
    Q_PROPERTY(QString mdbPreviewVersion READ mdbPreviewVersion NOTIFY mdbPreviewChanged)
    Q_PROPERTY(qint64 mdbPreviewSize READ mdbPreviewSize NOTIFY mdbPreviewChanged)

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
    QString dbcPreviewChannel() const { return m_dbcPreviewChannel; }
    QString dbcPreviewStatus() const { return m_dbcPreviewStatus; }
    QString dbcPreviewVersion() const { return m_dbcPreviewVersion; }
    qint64 dbcPreviewSize() const { return m_dbcPreviewSize; }
    QString mdbPreviewChannel() const { return m_mdbPreviewChannel; }
    QString mdbPreviewStatus() const { return m_mdbPreviewStatus; }
    QString mdbPreviewVersion() const { return m_mdbPreviewVersion; }
    qint64 mdbPreviewSize() const { return m_mdbPreviewSize; }

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
    void dbcPreviewChanged();
    void mdbPreviewChanged();

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
    QString m_dbcPreviewChannel;
    QString m_dbcPreviewStatus;
    QString m_dbcPreviewVersion;
    qint64 m_dbcPreviewSize = 0;
    QString m_mdbPreviewChannel;
    QString m_mdbPreviewStatus;
    QString m_mdbPreviewVersion;
    qint64 m_mdbPreviewSize = 0;
};

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlengine.h>

class InternetStore;
class OtaStore;
class SettingsService;
class SettingsStore;
class SystemInfoService;

// Owns the "switch release channel" flow the Settings > System > Updates menu
// starts. Everything the confirm screen shows comes from here.
//
// The download size is not computed locally: update-service knows the release
// index, the variant naming and the channel ordering rules, so each component
// is asked with a preview-channel: command and answers into the ota hash.
// This class fires both requests, watches OtaStore for the two answers, adds
// the sizes up, and gives up after a timeout so the screen never sits on
// "checking" forever when a component's update-service is absent or too old to
// know the command.
class UpdateChannelService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString targetChannel READ targetChannel NOTIFY targetChannelChanged)
    Q_PROPERTY(QString currentChannel READ currentChannel NOTIFY targetChannelChanged)
    Q_PROPERTY(QString version READ version NOTIFY stateChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY stateChanged)

public:
    enum State {
        Idle,        // no switch in progress
        Offline,     // no connectivity: the artifact has to come in by hand
        Checking,    // preview requested, waiting on the components
        Ready,       // version and totalBytes are the answer
        Unavailable, // the channel carries nothing for this scooter's variant
        Failed,      // the release index could not be reached
    };
    Q_ENUM(State)

    explicit UpdateChannelService(SettingsService *settingsService, SettingsStore *settings,
                                  OtaStore *ota, InternetStore *internet,
                                  SystemInfoService *systemInfo, QObject *parent = nullptr);

    int state() const { return static_cast<int>(m_state); }
    QString targetChannel() const { return m_targetChannel; }
    // The channel the scooter is on now, for the "Nightly -> Stable" line.
    QString currentChannel() const;
    QString version() const { return m_version; }
    qint64 totalBytes() const { return m_totalBytes; }

    // Starts a switch to channel. Offline is not an error here: it selects the
    // Offline state, whose screen explains the Update Mode route instead.
    Q_INVOKABLE void beginSwitch(const QString &channel);
    // Writes the channel on both components and asks them to check now, so the
    // download starts while the rider is still standing there rather than at
    // the next scheduled check.
    Q_INVOKABLE void confirm();
    Q_INVOKABLE void cancel();
    // True while either component is mid-update. Switching channels then would
    // queue a second, full download behind one already running.
    Q_INVOKABLE bool isUpdateInProgress() const;

signals:
    void stateChanged();
    void targetChannelChanged();
    // Emitted by confirm(), for whoever needs to leave the confirm screen.
    void switchConfirmed();

private:
    void setState(State s);
    void evaluatePreviews();
    bool isOnline() const;

    SettingsService *m_settingsService;
    SettingsStore *m_settings;
    OtaStore *m_ota;
    InternetStore *m_internet;
    SystemInfoService *m_systemInfo;

    State m_state = Idle;
    QString m_targetChannel;
    QString m_version;
    qint64 m_totalBytes = 0;
    QTimer m_timeout;
    // OtaStore applies a Redis snapshot field by field, emitting a change
    // signal for each. Evaluating on every one of those would read a
    // half-applied answer — status already "ready" while the size is still
    // whatever the last snapshot left. This coalesces the burst into one
    // evaluation after the whole snapshot has landed.
    QTimer m_evaluate;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static UpdateChannelService *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(UpdateChannelService *instance) { s_qmlInstance = instance; }

private:
    static inline UpdateChannelService *s_qmlInstance = nullptr;
};

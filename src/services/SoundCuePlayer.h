#pragma once

#include "SoundCueService.h"
#include <QObject>
#include <functional>
#include <memory>

class QThread;

// Constructed, initialized, used and destroyed exclusively on the audio thread.
class SoundCueBackend : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void initialize(const QString &assetRoot) = 0;
    virtual void play(SoundCue cue) = 0;
signals:
    void availableCuesChanged(quint32 mask);
};

class SoundCuePlayer : public QObject
{
    Q_OBJECT
public:
    using Factory = std::function<SoundCueBackend *()>;
    SoundCuePlayer(QString assetRoot, Factory factory, QObject *parent = nullptr,
                   int loadDelayMs = 8000);
    ~SoundCuePlayer() override;
    void play(SoundCue cue);
    void stop();
    bool available(SoundCue cue) const;
    int pendingCount() const;
    static constexpr int PulseDeadlineMs = 100;
    // Final process teardown only; never call while the HMI is running.
    static void drainAfterEventLoop();
private:
    struct Shared;
    class Worker;
    std::shared_ptr<Shared> m_shared;
    QThread *m_thread;
    Worker *m_worker;
    bool m_stopped = false;
};

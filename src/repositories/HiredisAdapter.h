#pragma once

#include <QObject>
#include <QSocketNotifier>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>

// Bridges a redisAsyncContext to the Qt event loop via QSocketNotifier.
// Attach to an existing async context with attach(). The adapter owns
// the notifiers and cleans them up when the context disconnects.
class HiredisAdapter : public QObject
{
    Q_OBJECT

public:
    explicit HiredisAdapter(QObject *parent = nullptr)
        : QObject(parent) {}

    // Wire up the async context to Qt's event loop.
    // Call this once after redisAsyncConnect() succeeds.
    bool attach(redisAsyncContext *ac)
    {
        if (!ac || ac->ev.data) return false;

        m_ctx = ac;
        ac->ev.data = this;
        ac->ev.addRead = [](void *p) {
            auto *self = static_cast<HiredisAdapter *>(p);
            if (self->m_read) self->m_read->setEnabled(true);
        };
        ac->ev.delRead = [](void *p) {
            auto *self = static_cast<HiredisAdapter *>(p);
            if (self->m_read) self->m_read->setEnabled(false);
        };
        ac->ev.addWrite = [](void *p) {
            auto *self = static_cast<HiredisAdapter *>(p);
            if (self->m_write) self->m_write->setEnabled(true);
        };
        ac->ev.delWrite = [](void *p) {
            auto *self = static_cast<HiredisAdapter *>(p);
            if (self->m_write) self->m_write->setEnabled(false);
        };
        ac->ev.cleanup = [](void *p) {
            auto *self = static_cast<HiredisAdapter *>(p);
            self->cleanup();
        };

        int fd = ac->c.fd;

        m_read = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        m_read->setEnabled(false);
        connect(m_read, &QSocketNotifier::activated, this, [this]() {
            if (m_ctx) redisAsyncHandleRead(m_ctx);
        });

        m_write = new QSocketNotifier(fd, QSocketNotifier::Write, this);
        m_write->setEnabled(false);
        connect(m_write, &QSocketNotifier::activated, this, [this]() {
            if (m_ctx) redisAsyncHandleWrite(m_ctx);
        });

        return true;
    }

private:
    void cleanup()
    {
        delete m_read;
        m_read = nullptr;
        delete m_write;
        m_write = nullptr;
        m_ctx = nullptr;
    }

    redisAsyncContext *m_ctx = nullptr;
    QSocketNotifier *m_read = nullptr;
    QSocketNotifier *m_write = nullptr;
};

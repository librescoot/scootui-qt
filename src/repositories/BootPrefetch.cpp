#include "BootPrefetch.h"

#include <QElapsedTimer>
#include <chrono>
#include <hiredis/hiredis.h>

BootPrefetch::BootPrefetch(QString host, quint16 port, QString backupHost,
                           QStringList channels, int deadlineMs)
    : m_host(std::move(host))
    , m_port(port)
    , m_backupHost(std::move(backupHost))
    , m_channels(std::move(channels))
    , m_deadlineMs(deadlineMs)
{
}

BootPrefetch::~BootPrefetch()
{
    m_cancel.store(true);
    if (m_thread.joinable())
        m_thread.join();
}

void BootPrefetch::start()
{
    if (m_thread.joinable())
        return;
    m_thread = std::thread([this] { run(); });
}

bool BootPrefetch::waitFinished(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_finished)
        return true;
    if (timeoutMs <= 0)
        return false;
    return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                         [this] { return m_finished; });
}

QHash<QString, FieldMap> BootPrefetch::take()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_succeeded.load())
        return {};
    return std::move(m_result);
}

static redisContext *connectWithin(const QString &host, quint16 port, int budgetMs)
{
    struct timeval tv = {budgetMs / 1000, (budgetMs % 1000) * 1000};
    redisContext *ctx = redisConnectWithTimeout(host.toUtf8().constData(), port, tv);
    if (ctx && ctx->err) {
        redisFree(ctx);
        return nullptr;
    }
    return ctx;
}

void BootPrefetch::run()
{
    QElapsedTimer elapsed;
    elapsed.start();

    QHash<QString, FieldMap> result;
    bool ok = false;
    bool backup = false;

    while (!m_cancel.load() && elapsed.elapsed() < m_deadlineMs) {
        redisContext *ctx = connectWithin(m_host, m_port, 200);
        if (!ctx && !m_backupHost.isEmpty()) {
            ctx = connectWithin(m_backupHost, m_port, 500);
            backup = ctx != nullptr;
        }
        if (!ctx) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        struct timeval commandTv = {0, 500 * 1000};
        redisSetTimeout(ctx, commandTv);

        result.clear();
        ok = true;
        for (const QString &channel : m_channels) {
            if (m_cancel.load()) {
                ok = false;
                break;
            }
            auto *reply = static_cast<redisReply *>(
                redisCommand(ctx, "HGETALL %s", channel.toUtf8().constData()));
            if (!reply || reply->type != REDIS_REPLY_ARRAY) {
                if (reply)
                    freeReplyObject(reply);
                ok = false;
                break;
            }
            FieldMap fields;
            fields.reserve(static_cast<int>(reply->elements / 2));
            for (size_t i = 0; i + 1 < reply->elements; i += 2) {
                fields.insert(QString::fromUtf8(reply->element[i]->str, reply->element[i]->len),
                              QString::fromUtf8(reply->element[i + 1]->str,
                                                reply->element[i + 1]->len));
            }
            result.insert(channel, fields);
            freeReplyObject(reply);
        }
        redisFree(ctx);
        if (ok)
            break;
        // A walk that broke off means the link dropped mid-way; start over.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (ok)
            m_result = std::move(result);
        m_succeeded.store(ok);
        m_usedBackup.store(ok && backup);
        m_finishedAtMs.store(elapsed.elapsed());
        m_finished = true;
    }
    m_cv.notify_all();
}

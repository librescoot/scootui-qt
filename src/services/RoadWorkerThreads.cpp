#include "RoadWorkerThreads.h"
#include <QCoreApplication>
#include <QList>
#include <QThread>

namespace {
QList<QThread *> &threads()
{
    static QList<QThread *> registry;
    return registry;
}
}

QThread *RoadWorkerThreads::create(const QString &name)
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    auto *thread = new QThread;
    thread->setObjectName(name);
    threads().append(thread);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    QObject::connect(thread, &QObject::destroyed, [thread]() {
        threads().removeOne(thread);
    });
    return thread;
}

void RoadWorkerThreads::drainAfterEventLoop()
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    const auto remaining = threads();
    for (auto *thread : remaining) {
        thread->requestInterruption();
        thread->quit();
    }
    for (auto *thread : remaining) {
        // No GUI events or queued completion callbacks are needed. Qt processes
        // worker deferred deletions at thread finish, including database closure.
        thread->wait();
        delete thread;
    }
}

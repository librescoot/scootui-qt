#pragma once

#include <QString>

class QThread;

namespace RoadWorkerThreads {
// GUI-thread-only registry, limited to the matcher and tile loader. Threads may
// outlive their service during normal operation; finished wrappers delete later.
QThread *create(const QString &name);

// Call only after the main event loop exits (or startup fails), with Qt still
// alive and live services already stopped. Unlike ordinary service destruction,
// final process teardown must join even orphaned workers before static teardown.
void drainAfterEventLoop();
}

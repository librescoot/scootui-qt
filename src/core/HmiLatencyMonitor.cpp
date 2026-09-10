#include "HmiLatencyMonitor.h"
#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QQuickWindow>
#include <chrono>

qint64 HmiLatencyMonitor::nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

HmiLatencyMonitor::HmiLatencyMonitor(QObject *parent) : QObject(parent)
{
    m_gui.start(nowMs());
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(HmiGuiLatencyMetrics::TickMs);
    connect(&m_timer, &QTimer::timeout, this, &HmiLatencyMonitor::sample);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &HmiLatencyMonitor::stop);
    m_timer.start();
}

void HmiLatencyMonitor::attachWindow(QQuickWindow *window)
{
    if (!window || m_window)
        return;
    m_window = window;
    window->installEventFilter(this);
    // These signals can run on the render thread. Capture only shared metrics,
    // never this/window; a callback already in flight remains safe at teardown.
    connect(window, &QQuickWindow::beforeSynchronizing, this, [frames = m_frames]() {
        frames->began(nowMs());
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::frameSwapped, this, [frames = m_frames]() {
        frames->swapped(nowMs());
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::visibleChanged, this, [frames = m_frames]() {
        frames->resetInterval();
    });
}

void HmiLatencyMonitor::stop()
{
    m_timer.stop();
    if (m_window) {
        m_window->removeEventFilter(this);
        disconnect(m_window, nullptr, this, nullptr);
    }
}

bool HmiLatencyMonitor::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        if (event->type() == QEvent::Expose)
            m_frames->resetInterval();
    }
    return QObject::eventFilter(watched, event);
}

void HmiLatencyMonitor::sample()
{
    const qint64 now = nowMs();
    m_gui.tick(now);
    if (!m_gui.reportDue(now))
        return;
    const qint64 gap = m_frames->maxSwapGap.exchange(0, std::memory_order_relaxed);
    const qint64 work = m_frames->maxFrameWork.exchange(0, std::memory_order_relaxed);
    const quint64 swaps = m_frames->swaps.exchange(0, std::memory_order_relaxed);
    if (m_gui.lateTicks || work >= 100) {
        qWarning() << "HMI latency: gui-late>=50ms" << m_gui.lateTicks
                   << "max-gui-late-ms" << m_gui.maxLate
                   << "swaps" << swaps << "max-swap-gap-ms" << gap
                   << "max-sync-to-swap-ms" << work
                   << "(swap gaps include idle; no swaps alone is not a stall)";
    }
    m_gui.lastReport = now;
    m_gui.maxLate = 0;
    m_gui.lateTicks = 0;
}

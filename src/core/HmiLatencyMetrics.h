#pragma once

#include <algorithm>
#include <atomic>
#include <QtGlobal>

// Fixed-size counters; render callbacks never enqueue GUI events. A swap gap
// alone is not evidence of a stall: Qt Quick does not render unchanged scenes.
struct HmiFrameMetrics {
    std::atomic<qint64> beganAt{0};
    std::atomic<qint64> lastSwap{0};
    std::atomic<qint64> maxSwapGap{0};
    std::atomic<qint64> maxFrameWork{0};
    std::atomic<quint64> swaps{0};

    static void maximum(std::atomic<qint64> &value, qint64 sample)
    {
        qint64 old = value.load(std::memory_order_relaxed);
        while (old < sample && !value.compare_exchange_weak(old, sample,
                                                           std::memory_order_relaxed)) {}
    }
    void began(qint64 now) { beganAt.store(now, std::memory_order_relaxed); }
    void swapped(qint64 now)
    {
        const qint64 previous = lastSwap.exchange(now, std::memory_order_relaxed);
        const qint64 begin = beganAt.exchange(0, std::memory_order_relaxed);
        if (previous)
            maximum(maxSwapGap, now - previous);
        if (begin)
            maximum(maxFrameWork, now - begin);
        swaps.fetch_add(1, std::memory_order_relaxed);
    }
    void resetInterval()
    {
        beganAt.store(0, std::memory_order_relaxed);
        lastSwap.store(0, std::memory_order_relaxed);
    }
};

struct HmiGuiLatencyMetrics {
    static constexpr qint64 TickMs = 100;
    static constexpr qint64 LateMs = 50;
    static constexpr qint64 ReportMs = 10000;
    qint64 expectedAt = 0;
    qint64 lastReport = 0;
    qint64 maxLate = 0;
    quint64 lateTicks = 0;

    void start(qint64 now) { expectedAt = now + TickMs; lastReport = now; }
    qint64 tick(qint64 now)
    {
        const qint64 late = std::max(qint64(0), now - expectedAt);
        expectedAt = now + TickMs; // Qt collapses missed timeouts; no catch-up burst.
        if (late >= LateMs) {
            ++lateTicks;
            maxLate = std::max(maxLate, late);
        }
        return late;
    }
    bool reportDue(qint64 now) const { return now - lastReport >= ReportMs; }
};

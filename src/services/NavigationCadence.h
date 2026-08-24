#pragma once

namespace NavigationCadence {

constexpr int RenderTickMs = 50;          // 20 Hz camera/estimator
constexpr int NavigationEveryTicks = 4;   // 5 Hz TBT + off-route state
constexpr int RoadInfoEveryTicks = 20;    // 1 Hz vector-tile match

static_assert(RenderTickMs * NavigationEveryTicks == 200);
static_assert(RenderTickMs * RoadInfoEveryTicks == 1000);

class TickDivider
{
public:
    explicit TickDivider(int ticksPerUpdate) : m_period(ticksPerUpdate) {}

    bool advance()
    {
        if (++m_phase < m_period)
            return false;
        m_phase = 0;
        return true;
    }

    void reset() { m_phase = 0; }

private:
    int m_period;
    int m_phase = 0;
};

} // namespace NavigationCadence

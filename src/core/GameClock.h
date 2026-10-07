#pragma once

namespace mc {

// Vanilla runs the simulation at a fixed 20 ticks per second.
inline constexpr int kTicksPerSecond = 20;
inline constexpr double kTickSeconds = 1.0 / kTicksPerSecond;

// Fixed-timestep accumulator: call advance() once per frame, then tick
// `ticksDue` times; render with `alpha` to interpolate between ticks.
class GameClock {
public:
    // Caps catch-up after a stall (vanilla also skips ticks when far behind).
    static constexpr int kMaxTicksPerFrame = 10;

    void advance(double frameSeconds) {
        m_accumulator += frameSeconds;
        ticksDue = 0;
        while (m_accumulator >= kTickSeconds && ticksDue < kMaxTicksPerFrame) {
            m_accumulator -= kTickSeconds;
            ++ticksDue;
        }
        if (ticksDue == kMaxTicksPerFrame) m_accumulator = 0.0;
        alpha = m_accumulator / kTickSeconds;
    }

    int ticksDue = 0;
    double alpha = 0.0;

private:
    double m_accumulator = 0.0;
};

} // namespace mc

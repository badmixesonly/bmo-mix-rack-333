#pragma once

#include <array>
#include <atomic>
#include <cmath>

namespace bmo
{

/** Peak and RMS of the last block, per channel, published from the audio
    thread and polled by a panel on a timer. Publish-and-sample; never push
    from audio to UI. */
class Meter
{
public:
    void reset() noexcept
    {
        for (auto& a : { &peak, &rms })
            for (auto& v : *a)
                v.store (0.0f, std::memory_order_relaxed);
    }

    void measure (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            if (ch >= numChannels || numSamples <= 0)
            {
                peak[(size_t) ch].store (0.0f, std::memory_order_relaxed);
                rms [(size_t) ch].store (0.0f, std::memory_order_relaxed);
                continue;
            }

            float p = 0.0f;
            double s = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto v = channels[ch][i];
                p = std::max (p, std::abs (v));
                s += (double) v * v;
            }

            peak[(size_t) ch].store (p, std::memory_order_relaxed);
            rms [(size_t) ch].store ((float) std::sqrt (s / (double) numSamples),
                                     std::memory_order_relaxed);
        }
    }

    float getPeak (int ch) const noexcept { return peak[(size_t) (ch == 0 ? 0 : 1)].load (std::memory_order_relaxed); }
    float getRms  (int ch) const noexcept { return rms [(size_t) (ch == 0 ? 0 : 1)].load (std::memory_order_relaxed); }

    float maxPeak() const noexcept { return std::max (getPeak (0), getPeak (1)); }
    float maxRms()  const noexcept { return std::max (getRms (0),  getRms (1)); }

private:
    std::array<std::atomic<float>, 2> peak { }, rms { };
};

} // namespace bmo

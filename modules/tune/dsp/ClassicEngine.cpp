#include "modules/tune/dsp/ClassicEngine.h"
#include <algorithm>
#include <cmath>

namespace bmo::tune
{

namespace
{
    /** The splice guard: how far below its window's floor the outgoing read
        of a crossfade can drift while raising pitch, as a fraction of the
        period (half a period of fade at the design ratio). */
    double guardFraction()
    {
        return 0.5 * (std::exp2 (ClassicEngine::kDesignCents / 1200.0) - 1.0);
    }
}

int ClassicEngine::latencyFor (bool studioMode, double rangePeriod) noexcept
{
    if (! studioMode)
        return 0;

    // Centre the window half a period plus the splice guard above the floor,
    // so the lowest note of the range can swing a full period and still
    // never read a sample that has not arrived.
    return kFloor + (int) std::ceil (rangePeriod * (0.5 + guardFraction()));
}

void ClassicEngine::prepare (double sampleRate, double longestPeriod)
{
    fs = sampleRate;

    // The deepest read: the Studio rest lag for the longest period, plus a
    // period of swing, plus a fade's drift, plus the kernel.
    const auto deepest = latencyFor (true, longestPeriod) + 2.0 * longestPeriod + 2 * Sinc::kTaps + 64;
    int size = 1;
    while (size < (int) deepest)
        size <<= 1;

    ring.assign ((size_t) size, 0.0f);
    mask = size - 1;

    // A read at rate rho folds content at f to fs - rho f. Below this ratio
    // the fold cannot reach 20 kHz, so the full-band kernel -- the one that
    // is a pure delay at rest -- is kept. 1.167 (+267 cents) at 48 kHz,
    // 1.093 (+154) at 44.1; at 88.2 and up every correction the engine is
    // specified for stays full band.
    fullBandRatio = std::max (1.0, 2.0 * (1.0 - 20000.0 / fs));

    tables[0].build (1.0, 8.0);
    for (int b = 1; b < (int) tables.size(); ++b)
    {
        const auto upper = std::exp2 (b * 100.0 / 1200.0);
        tables[(size_t) b].build (upper <= fullBandRatio ? 1.0 : 0.90 / upper, 8.0);
    }

    reset();
}

void ClassicEngine::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    write = 0;
    lag = restLag;
    ratio = 1.0;
    lastPeriod = 0.0;
    fading = false;
    spliced = false;
    splices = 0;
}

void ClassicEngine::setLatencyMode (bool studioMode, double rangePeriod) noexcept
{
    const auto rest = studioMode ? latencyFor (true, rangePeriod) : kLiveRest;

    if (studioMode == studio && rest == restLag)
        return;

    studio = studioMode;
    restLag = rest;

    // A contract change is a discontinuity by definition; fade to the new
    // rest position rather than jump.
    startFade (restLag, std::max (16, (int) (0.005 * fs)), true);
}

const ClassicEngine::Sinc& ClassicEngine::tableFor (double rho) const noexcept
{
    if (rho <= 1.0)
        return tables[0];

    const auto cents = 1200.0 * std::log2 (rho);
    const auto b = std::clamp ((int) std::ceil (cents / 100.0), 1, (int) tables.size() - 1);
    return tables[(size_t) b];
}

double ClassicEngine::read (const Sinc& t, double lagBehindNewest) const noexcept
{
    const auto newest = write - 1;
    return t.read (ring.data(), mask, (double) newest - lagBehindNewest);
}

void ClassicEngine::startFade (double newLag, int length, bool equalPower) noexcept
{
    // A fade already running is abandoned where it is: its outgoing read is
    // replaced by the incoming one, which is what the listener was hearing
    // most of by then anyway.
    fadeLag = lag;
    lag = newLag;
    fading = true;
    fadeEqualPower = equalPower;
    fadeLength = std::max (1, length);
    fadePosition = 0;
}

float ClassicEngine::process (float input, double cents, double period, bool settled) noexcept
{
    ring[(size_t) write] = std::isfinite (input) ? input : 0.0f;
    write = (write + 1) & mask;
    spliced = false;

    ratio = std::exp2 (std::clamp (cents, -1200.0, 1200.0) / 1200.0);
    if (period > 1.0)
        lastPeriod = period;

    const auto step = 1.0 - ratio;
    lag += step;
    if (fading)
        fadeLag = std::max ((double) kFloor, fadeLag + step);

    if (lastPeriod > 1.0 && ! fading)
    {
        const auto T = lastPeriod;
        const auto fade = std::max (16, (int) std::lround (0.5 * T));

        // The window. In Live its floor is raised by however far the
        // outgoing read will drift during a fade at the ratio in force, so
        // a splice never asks the kernel for a sample that has not arrived.
        double lo, hi;
        if (studio)
        {
            lo = restLag - 0.5 * T;
            hi = restLag + 0.5 * T;
        }
        else
        {
            lo = kFloor + fade * std::max (0.0, ratio - 1.0);
            hi = kLiveRest + T;
        }

        if (lag < lo || lag > hi)
        {
            // Whole periods only: the waveform one cycle away is the same
            // waveform. Usually one; more after a leap to a much higher note
            // shrank the window underneath the pointer.
            const auto k = lag < lo ? std::ceil ((lo - lag) / T) : -std::ceil ((lag - hi) / T);
            startFade (lag + k * T, fade, false);
            spliced = true;
            ++splices;
        }
        else if (settled && std::abs (lag - restLag) > 0.5)
        {
            // Home, over 5 ms of uncorrelated material.
            startFade (restLag, std::max (16, (int) (0.005 * fs)), true);
        }
    }
    else if (lastPeriod <= 1.0 && settled && ! fading && std::abs (lag - restLag) > 0.5)
    {
        startFade (restLag, std::max (16, (int) (0.005 * fs)), true);
    }

    lag = std::max ((double) kFloor, lag);

    const auto& table = tableFor (ratio);
    auto y = read (table, lag);

    if (fading)
    {
        const auto outgoing = read (table, fadeLag);
        const auto t = (double) (fadePosition + 1) / (double) (fadeLength + 1);

        if (fadeEqualPower)
        {
            const auto a = std::cos (0.5 * kPi * t), b = std::sin (0.5 * kPi * t);
            y = a * outgoing + b * y;
        }
        else
        {
            y = (1.0 - t) * outgoing + t * y;
        }

        if (++fadePosition >= fadeLength)
            fading = false;
    }

    return std::isfinite (y) ? (float) y : 0.0f;
}

} // namespace bmo::tune

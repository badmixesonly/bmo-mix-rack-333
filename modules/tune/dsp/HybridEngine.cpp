#include "modules/tune/dsp/HybridEngine.h"
#include <algorithm>
#include <cmath>

namespace bmo::tune
{

void HybridEngine::prepare (double sampleRate, double longestPeriod)
{
    fs = sampleRate;

    // The deepest read: the Studio delay for the longest period, plus a
    // period of window swing, plus a grain's half-length each side read at up
    // to twice real time, plus the kernel.
    const auto deepest = contract::studio (longestPeriod) + 4.0 * longestPeriod + 2 * SincBank::Table::kTaps + 64;
    int size = 1;
    while (size < (int) deepest)
        size <<= 1;

    input.assign ((size_t) size, 0.0f);
    mask = size - 1;
    kernels.build (fs);
    reset();
}

void HybridEngine::reset()
{
    std::fill (input.begin(), input.end(), 0.0f);
    now = -1;
    grainCount = 0;
    nextSynthesis = nextAnalysis = 0.0;
    heldPeriod = fs / 200.0;
    started = false;
    scheduled = 0;
    ratio = 1.0;
    lastDelay = studio ? contract::studio (rangePeriodCurrent) : contract::kLiveRest;
    settledNow = false;
}

void HybridEngine::setLatencyMode (bool studioMode, double rangePeriod) noexcept
{
    // No fade needed, unlike CLASSIC: the next grain is simply taken at the
    // new delay, and grains overlap by construction.
    studio = studioMode;
    rangePeriodCurrent = rangePeriod;
}

void HybridEngine::setFormant (bool correct, double shiftCents) noexcept
{
    formantCorrect = correct;
    formantRatio = std::exp2 (std::clamp (shiftCents, -1200.0, 1200.0) / 1200.0);
}

void HybridEngine::feed (float x, double period) noexcept
{
    ++now;
    input[(size_t) (now & mask)] = std::isfinite (x) ? x : 0.0f;

    if (period > 1.0)
        heldPeriod = period;

    grainCount = 0;
    started = false;
    ratio = 1.0;
    lastDelay = studio ? contract::studio (rangePeriodCurrent) : contract::kLiveRest;
}

void HybridEngine::scheduleGrains (double period) noexcept
{
    if (period > 1.0)
        heldPeriod = period;

    const auto T = std::max (8.0, heldPeriod);
    const auto grainRate = std::clamp ((formantCorrect ? 1.0 : ratio) * formantRatio, 0.5, 2.0);
    const auto halfIn = T;
    const auto halfOut = halfIn / grainRate;

    // A grain read faster than real time reaches further ahead than its
    // centre by the end; keep that inside the kernel's lookahead too.
    const auto readAhead = grainRate > 1.0 ? halfIn * (1.0 - 1.0 / grainRate) : 0.0;
    const auto rest = studio ? (double) contract::studio (rangePeriodCurrent) : (double) contract::kLiveRest;

    if (! started)
    {
        nextSynthesis = (double) now;
        nextAnalysis = nextSynthesis - rest;
        started = true;
    }

    while (nextSynthesis - halfOut <= (double) now + 1.0 && grainCount < (int) grains.size())
    {
        auto delay = nextSynthesis - nextAnalysis;

        if (settledNow)
        {
            // Nothing pitched to stay synchronous with: go home to the rest
            // delay. The grains overlap, so the move is itself a crossfade.
            nextAnalysis = nextSynthesis - rest;
        }
        else
        {
            // The same window as CLASSIC's. Each grain's delay drifts by
            // T - T/ratio; when it leaves the window the analysis position
            // steps a whole period back (a cycle used twice) or forward (a
            // cycle dropped). Whole periods only, so each grain stays one
            // cycle from its neighbour.
            const auto lo = studio ? std::max (rest - 0.5 * T, contract::kFloor + readAhead)
                                   : contract::kLiveRest + readAhead;
            const auto hi = studio ? rest + 0.5 * T : lo + T;

            while (delay < lo) { nextAnalysis -= T; delay += T; }
            while (delay > hi) { nextAnalysis += T; delay -= T; }
        }

        grains[(size_t) grainCount++] = { nextSynthesis, nextAnalysis, halfOut, grainRate };
        ++scheduled;

        // Analysis advances one period per grain; synthesis advances one
        // *target* period. This is the only place the correction reaches the
        // grains, and at ratio 1 the two advance together and the delay
        // never moves.
        nextAnalysis += T;
        nextSynthesis += T / std::clamp (ratio, 0.5, 2.0);
    }
}

float HybridEngine::process (float x, double cents, double period, bool settled) noexcept
{
    ++now;
    input[(size_t) (now & mask)] = std::isfinite (x) ? x : 0.0f;

    ratio = std::exp2 (std::clamp (cents, -1200.0, 1200.0) / 1200.0);
    settledNow = settled;
    scheduleGrains (period);

    // Overlap-add, normalised by the summed window.
    double acc = 0.0, weight = 0.0, strongest = -1.0;
    int kept = 0;
    const auto newestReadable = (double) (now - SincBank::Table::kLookahead);

    for (int g = 0; g < grainCount; ++g)
    {
        const auto gr = grains[(size_t) g];
        const auto offset = (double) now - gr.centreOut;

        if (offset >= gr.halfOut)
            continue;   // finished: dropped

        grains[(size_t) kept++] = gr;

        const auto t = offset / gr.halfOut;
        if (t <= -1.0)
            continue;   // scheduled, not yet begun

        const auto w = 0.5 + 0.5 * std::cos (kPi * t);
        const auto position = std::min (gr.centreIn + gr.rate * offset, newestReadable);
        acc += w * kernels.forRatio (gr.rate).read (input.data(), mask, position);
        weight += w;

        if (w > strongest)
        {
            strongest = w;
            lastDelay = (double) now - position;
        }
    }

    grainCount = kept;
    const auto y = weight > 1.0e-3 ? acc / weight : 0.0;
    return std::isfinite (y) ? (float) y : 0.0f;
}

} // namespace bmo::tune

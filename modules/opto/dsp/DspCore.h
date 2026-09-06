#pragma once

#include "Detector.h"
#include <array>
#include <cmath>

namespace bmo::opto
{

/** One-pole parameter smoother, same shape as the one in modules/sat/dsp --
    see that file for why: snaps to the target once it is within epsilon, so
    a settled parameter compares exactly equal. CRUSH and LEVEL are steady
    knobs most of the time, but a session can still automate either, and
    stepping the curve or the makeup gain block-to-block with no ramp is an
    audible zipper. */
class Smoother
{
public:
    void prepare (double sampleRate, double timeMs) noexcept
    {
        const auto tau = std::max (timeMs, 0.01) * 0.001;
        coeff = (float) (1.0 - std::exp (-1.0 / (std::max (sampleRate, 1.0) * tau)));
    }

    void snap (float v) noexcept      { current = target = v; }
    void setTarget (float t) noexcept { target = t; }

    float tick() noexcept
    {
        current += coeff * (target - current);

        if (std::abs (target - current) < 1.0e-5f)
            current = target;

        return current;
    }

private:
    float coeff = 1.0f, current = 0.0f, target = 0.0f;
};

//==============================================================================
/** BMO Opto: input -> the cell (feedback opto-style compressor, driven by
    CRUSH) -> makeup gain (LEVEL) -> output. See Detector.h for the cell
    itself; this class is the block-level wrapper: per-channel state, param
    smoothing, and the gain-reduction figure the panel's meter reads.

    No added colour on purpose. The brief this was built from describes five
    stages -- input level, the cell, gain-reduction level, makeup gain, output
    level -- and nothing else; a saturation stage would be an addition to
    that brief, not an implementation of it, so it is left out rather than
    guessed at. */
class DspCore
{
public:
    struct Params
    {
        float crushPercent = 35.0f;
        float levelDb      = 0.0f;
    };

    void prepare (double newSampleRate, int /*maxBlockSize*/, int numChannels) noexcept
    {
        rate = newSampleRate;
        numActiveChannels = std::clamp (numChannels, 1, (int) channels.size());

        for (auto& ch : channels)
            ch.detector.prepare (rate);

        crushSmoother.prepare (rate, 15.0);
        levelSmoother.prepare (rate, 15.0);
        crushSmoother.snap (params.crushPercent);
        levelSmoother.snap (params.levelDb);

        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : channels)
            ch.detector.reset();

        reportedReductionDb = 0.0f;
    }

    void setParams (const Params& p) noexcept
    {
        params = p;
        crushSmoother.setTarget (p.crushPercent);
        levelSmoother.setTarget (p.levelDb);
    }

    void process (float* const* channelData, int numChannels, int numSamples) noexcept
    {
        const auto active = std::min (numChannels, numActiveChannels);
        auto blockMaxReduction = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto curve    = OptoDetector::curveFor (crushSmoother.tick());
            const auto makeupLin = std::pow (10.0f, levelSmoother.tick() / 20.0f);

            for (int ch = 0; ch < active; ++ch)
            {
                auto& detector = channels[(size_t) ch].detector;
                const auto reduced = detector.process (channelData[ch][i], curve);

                channelData[ch][i] = reduced * makeupLin;
                blockMaxReduction  = std::max (blockMaxReduction, detector.currentReductionDb());
            }
        }

        reportedReductionDb = blockMaxReduction;
    }

    /** The worst (largest) gain reduction seen in the block just processed,
        in dB, always >= 0. Called from the audio thread immediately after
        process(), same as core/dsp/Meter.h's own publish-and-sample rule --
        see core/product/ModuleEngine.h. */
    float currentGainReductionDb() const noexcept { return reportedReductionDb; }

private:
    struct Channel { OptoDetector detector; };

    double rate = 44100.0;
    int numActiveChannels = 2;

    std::array<Channel, 2> channels;
    Smoother crushSmoother, levelSmoother;

    Params params;
    float reportedReductionDb = 0.0f;
};

} // namespace bmo::opto

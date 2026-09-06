#pragma once

#include "Detector.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace bmo::opto
{

enum class Mode { La2a = 0, Distressor = 1 };

/** One-pole parameter smoother, same shape as the one in modules/sat/dsp --
    see that file for why: snaps to the target once it is within epsilon, so
    a settled parameter compares exactly equal. CRUSH and LEVEL are steady
    knobs most of the time, but a session can still automate either, and
    stepping the curve or the makeup gain block-to-block with no ramp is an
    audible zipper. Mode/Link/Drive are discrete switches, not smoothed --
    same convention modules/sat/dsp uses for its own toggles. */
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
/** BMO Opto: input -> the cell -> makeup gain (LEVEL) -> drive (optional) ->
    output.

    Mode picks a genuinely different signal path, not just different curve
    numbers: LA-2A mode runs a feedback cell (La2aCell), Distressor mode runs
    a feedforward one (DistressorCell) -- see Detector.h for why each is
    built the way it is. Link shares one cell's gain reduction across both
    channels instead of letting them compress independently; it is orthogonal
    to Mode, so it dispatches on whichever cell Mode has already selected.
    Color is an on/off harmonic stage, mode-flavoured the same way -- except
    Tele mode has no off switch for it: `params.color` is only honoured in
    Stressed mode, Tele always runs its drive stage. That's a placeholder
    product decision (see params.h), enforced here so it holds regardless of
    what the panel does or doesn't grey out. */
class DspCore
{
public:
    struct Params
    {
        float crushPercent = 35.0f;
        float levelDb      = 0.0f;
        Mode  mode         = Mode::La2a;
        bool  link         = true;
        bool  color        = false;
    };

    void prepare (double newSampleRate, int /*maxBlockSize*/, int numChannels) noexcept
    {
        rate = newSampleRate;
        numActiveChannels = std::clamp (numChannels, 1, (int) channels.size());

        for (auto& ch : channels)
        {
            ch.la2a.prepare (rate);
            ch.distressor.prepare (rate);
        }

        crushSmoother.prepare (rate, 15.0);
        levelSmoother.prepare (rate, 15.0);
        crushSmoother.snap (params.crushPercent);
        levelSmoother.snap (params.levelDb);

        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : channels)
        {
            ch.la2a.reset();
            ch.distressor.reset();
            ch.la2aDrive.reset();
            ch.distressorDrive.reset();
        }

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
        const auto mode   = params.mode;
        auto blockMaxReduction = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto curve     = mode == Mode::La2a ? curveForLa2a (crushSmoother.tick())
                                                        : curveForDistressor (crushSmoother.tick());
            const auto makeupLin = std::pow (10.0f, levelSmoother.tick() / 20.0f);

            const auto reduction = params.link && active >= 2
                                      ? processLinked (channelData, i, mode, curve)
                                      : processIndependent (channelData, i, active, mode, curve);

            blockMaxReduction = std::max (blockMaxReduction, reduction);

            // Tele has no off switch for its drive stage -- it always runs.
            // Stressed's is a real toggle. See the class comment.
            const auto colorActive = mode == Mode::La2a || params.color;

            for (int ch = 0; ch < active; ++ch)
            {
                auto& c = channels[(size_t) ch];
                auto v  = channelData[ch][i] * makeupLin;

                if (colorActive)
                    v = mode == Mode::La2a ? c.la2aDrive.process (v) : c.distressorDrive.process (v);

                channelData[ch][i] = v;
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
    struct Channel
    {
        La2aCell        la2a;
        DistressorCell  distressor;
        La2aDrive       la2aDrive;
        DistressorDrive distressorDrive;
    };

    /** Both channels compress independently -- today's default topology,
        each channel's own cell reacting only to its own signal. */
    float processIndependent (float* const* channelData, int i, int active, Mode mode, const Curve& curve) noexcept
    {
        auto worst = 0.0f;

        for (int ch = 0; ch < active; ++ch)
        {
            auto& c = channels[(size_t) ch];

            if (mode == Mode::La2a)
            {
                channelData[ch][i] = c.la2a.process (channelData[ch][i], curve);
                worst = std::max (worst, c.la2a.currentReductionDb());
            }
            else
            {
                channelData[ch][i] = c.distressor.process (channelData[ch][i], curve);
                worst = std::max (worst, c.distressor.currentReductionDb());
            }
        }

        return worst;
    }

    /** One shared cell (channel 0's) decides a single gain reduction from
        the louder of the two channels, applied identically to both -- like
        a real stereo-linked pair sharing one control voltage. */
    float processLinked (float* const* channelData, int i, Mode mode, const Curve& curve) noexcept
    {
        auto& shared = channels[0];
        const auto l = channelData[0][i];
        const auto r = channelData[1][i];

        if (mode == Mode::La2a)
        {
            // Feedback: detection reads the *output*, so derive both
            // channels' outputs from the gain the previous sample decided,
            // pick the louder for detection, then update from that.
            const auto g = shared.la2a.currentGainLin();
            const auto lOut = l * g, rOut = r * g;
            shared.la2a.updateFromOutputSample (std::abs (lOut) > std::abs (rOut) ? lOut : rOut, curve);

            channelData[0][i] = lOut;
            channelData[1][i] = rOut;
            return shared.la2a.currentReductionDb();
        }

        const auto detect = std::abs (l) > std::abs (r) ? l : r;
        shared.distressor.updateFromInputSample (detect, curve);
        const auto g = shared.distressor.currentGainLin();

        channelData[0][i] = l * g;
        channelData[1][i] = r * g;
        return shared.distressor.currentReductionDb();
    }

    double rate = 44100.0;
    int numActiveChannels = 2;

    std::array<Channel, 2> channels;
    Smoother crushSmoother, levelSmoother;

    Params params;
    float reportedReductionDb = 0.0f;
};

} // namespace bmo::opto

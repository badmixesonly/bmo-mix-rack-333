#pragma once

#include "Crossover.h"
#include "Detector.h"
#include "Gate.h"
#include "Limiter.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace bmo::vcomp
{

/** One-pole parameter smoother, the same shape as the ones in modules/sat/dsp
    and modules/opto/dsp -- see either for why it snaps to the target inside an
    epsilon, so a settled parameter compares exactly equal.

    AMOUNT and OUTPUT get one each. Both are steady knobs most of the time, but
    either can be automated, and AMOUNT moves the threshold, the knee, the
    ratio and the makeup gain at once -- stepping all four block to block with
    no ramp is an audible zipper on the one control anybody will ride. */
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
/** BMO Vcomp.

        in -> gate -> [band split] -> compressor on the mid band
           -> + the thru bands -> auto makeup -> OUTPUT -> limiter

    **The gate is first**, because what it closes has to be closed before the
    makeup amplifies it, and it is keyed off the raw input so its threshold is
    an absolute level rather than one that moves with AMOUNT. See Gate.h.

    **The detector reads the full gated signal through the sidechain high-pass,
    not the mid band.** SIDECHAIN and LOW THRU are deliberately two controls
    doing two jobs -- what the compressor listens to, and what it acts on -- and
    keying the detector off the mid band would quietly merge them, so that
    moving LOW THRU would also change how hard the compressor works. It does
    not: LOW THRU changes only which parts of the signal the gain is applied
    to.

    **Standard mode ignores six parameters.** With COMPLEX off, ATTACK,
    RELEASE, ARC, SIDECHAIN, LOW THRU and HIGH THRU are not read at all -- the
    figures in params.h are used instead. The lock is here rather than in the
    panel, the same way BMO Opto's Color-in-Tele lock is in its DspCore, so it
    holds for the rack, for a preset, for automation and for the tests, not
    just for someone looking at the panel. Because those figures are also the
    six parameters' defaults, switching COMPLEX on with untouched knobs is
    silent.

    **Stereo is always linked**, and there is no switch for it. Two channels of
    one voice compressed independently is a wandering image, not a stereo
    option, and this module is for voices. One detector reads the louder of the
    two channels and both get the same gain; the gate is linked the same way,
    so one channel never opens without the other. Compare BMO Opto, which does
    have a LINK switch because it is a general-purpose box that ends up across
    a mix. */
class DspCore
{
public:
    struct Params
    {
        float amountPercent = 0.0f;
        float gateDb        = kGateOffDb;
        float outputDb      = 0.0f;
        bool  complex       = false;
        float attackMs      = kStandardAttackMs;
        float releaseMs     = kStandardReleaseMs;
        bool  arc           = kStandardArc;
        float sidechainHz   = kStandardSidechainHz;
        float lowThruHz     = kStandardLowThruHz;
        float highThruHz    = kStandardHighThruHz;
    };

    void prepare (double newSampleRate, int /*maxBlockSize*/, int numChannels) noexcept
    {
        rate = newSampleRate;
        numActiveChannels = std::clamp (numChannels, 1, (int) channels.size());

        for (auto& ch : channels)
        {
            ch.sidechain.prepare (rate);
            ch.bands.prepare (rate);
        }

        gate.prepare (rate);
        limiter.prepare (rate);

        amountSmoother.prepare (rate, 15.0);
        outputSmoother.prepare (rate, 15.0);
        amountSmoother.snap (params.amountPercent);
        outputSmoother.snap (params.outputDb);

        applyTimings();
        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : channels)
        {
            ch.sidechain.reset();
            ch.bands.reset();
        }

        gate.reset();
        limiter.reset();
        release.reset();

        envelopeDb = 0.0f;
        reportedReductionDb = 0.0f;
    }

    void setParams (const Params& p) noexcept
    {
        params = p;
        amountSmoother.setTarget (p.amountPercent);
        outputSmoother.setTarget (p.outputDb);
        applyTimings();
    }

    /** True when either band control is off its rail, and therefore when the
        crossover is in circuit at all. Both at their rails means the whole
        split is skipped rather than run with empty outer bands -- a crossover
        left in costs its allpass phase shift whether or not anything is in the
        bands it made. */
    static bool bandsActive (const Params& p) noexcept
    {
        if (! p.complex)
            return false;

        return p.lowThruHz > kLowThruOffHz || p.highThruHz < kHighThruOffHz;
    }

    void process (float* const* channelData, int numChannels, int numSamples) noexcept
    {
        const auto active = std::min (numChannels, numActiveChannels);
        const auto split  = bandsActive (params);

        auto blockMaxReduction = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto curve = curveFor (amountSmoother.tick());

            // **The automatic makeup applies to the whole sum, including the
            // bands the compressor did not touch.** Frosty's spec, 2026-09-14:
            // LOW THRU picks a frequency, everything above it is compressed
            // and everything below is not, and *both* get the makeup.
            //
            // This was the other way round for one build, on the reasoning
            // that makeup gives back what the curve took and the thru bands
            // had nothing taken. That is true and it is not what a listener
            // judges. With makeup on the compressed band alone the thru band
            // sits at input level while the compressed band is lifted, so
            // engaging LOW THRU made the voice *thinner* the harder AMOUNT was
            // pushed -- measured at -1.3 dB of tilt at AMOUNT 30 and -4.3 dB
            // at 90. A control called LOW THRU that removes low end is the
            // opposite of the thing, which is how the ear found it.
            //
            // The cost is the other direction and it is bigger: an
            // uncompressed band taking full makeup can only get louder, by the
            // whole makeup figure. That is +6 dB of low end at AMOUNT 30 and
            // +25 at 90 (measure_vcomp balance). The feature is therefore
            // usable at low to middling AMOUNT and self-defeating above it --
            // a property of what was asked for rather than of this
            // implementation, and recorded in modules/vcomp/AGENTS.md as
            // something an ear has to rule on.
            //
            // OUTPUT multiplies the sum either way: it is the user's trim on
            // the whole module.
            const auto autoMakeupLin = std::pow (10.0f, autoMakeupDb (curve) / 20.0f);
            const auto outputLin     = std::pow (10.0f, outputSmoother.tick() / 20.0f);

            // The gate is keyed off the raw input, before anything else.
            // The IN meter the gate handle sits on is the engine's own input
            // meter, which is taken at the same point -- so the handle is
            // dragged against the level it is actually judging.
            auto inputPeak = 0.0f;

            for (int ch = 0; ch < active; ++ch)
                inputPeak = std::max (inputPeak, std::abs (channelData[ch][i]));

            // One gate for both channels, off the louder of the two: a gate
            // that opened on one channel only would swing the image.
            const auto gateGain = gate.process (levelDbOf (inputPeak));

            // One detector for both channels, off the louder after each
            // channel's own sidechain high-pass.
            auto detected = 0.0f;

            for (int ch = 0; ch < active; ++ch)
                detected = std::max (detected,
                                     std::abs (channels[(size_t) ch].sidechain.process (channelData[ch][i] * gateGain)));

            const auto demandDb = kneeReductionDb (levelDbOf (detected), curve);

            // Smooth decoupled peak detector: the release stage takes the
            // demand instantly, then one attack pole shapes the whole thing.
            // See ReleaseStage in Detector.h for why the attack lives out here
            // and not inside the branches.
            const auto released = release.tick (demandDb);
            envelopeDb = attackPole * envelopeDb + (1.0f - attackPole) * released;

            if (envelopeDb < kEnvelopeFloorDb)
                envelopeDb = 0.0f;

            blockMaxReduction = std::max (blockMaxReduction, envelopeDb);

            const auto compressorGain = std::pow (10.0f, -envelopeDb / 20.0f);

            // Both channels are worked out before either is written, because
            // the limiter needs the peak of the pair to decide one gain for
            // both. Writing as we went and limiting afterwards would either
            // limit each channel on its own -- which swings the image exactly
            // when the signal is loudest -- or need a second pass over the
            // samples just written.
            float pending[2] { 0.0f, 0.0f };
            auto pendingPeak = 0.0f;

            for (int ch = 0; ch < active; ++ch)
            {
                auto& c = channels[(size_t) ch];
                const auto gated = channelData[ch][i] * gateGain;

                float out;

                if (split)
                {
                    float mid = 0.0f, thru = 0.0f;
                    c.bands.process (gated, mid, thru);
                    out = (mid * compressorGain + thru) * autoMakeupLin * outputLin;
                }
                else
                {
                    out = gated * compressorGain * autoMakeupLin * outputLin;
                }

                pending[(size_t) ch] = out;
                pendingPeak = std::max (pendingPeak, std::abs (out));
            }

            // Last, and after OUTPUT: the ceiling is the module's, so OUTPUT
            // drives into it rather than sitting past it. A trim that could
            // push the output over the ceiling would make the ceiling a
            // suggestion.
            const auto limiterGain = limiter.process (pendingPeak);

            for (int ch = 0; ch < active; ++ch)
                channelData[ch][i] = pending[(size_t) ch] * limiterGain;
        }

        reportedReductionDb = blockMaxReduction;
    }

    /** The worst (largest) gain reduction seen in the block just processed, in
        dB, always >= 0. Read from the audio thread immediately after
        process() -- see core/product/ModuleEngine.h.

        This is the *compressor's* reduction and does not include the gate.
        The gate is a separate stage doing a different job, and folding its
        attenuation in would make the GR meter read 50 dB every time the singer
        stops, which says nothing about how hard the compressor is working. */
    float currentGainReductionDb() const noexcept { return reportedReductionDb; }


private:
    struct Channel
    {
        SidechainHighpass sidechain;
        BandSplit bands;
    };

    /** The one place standard mode's figures are substituted for the
        parameters. Everything downstream reads the result and cannot tell
        which it got, which is the point. */
    void applyTimings() noexcept
    {
        const auto attackMs    = params.complex ? params.attackMs    : kStandardAttackMs;
        const auto releaseMs   = params.complex ? params.releaseMs   : kStandardReleaseMs;
        const auto arcOn       = params.complex ? params.arc         : kStandardArc;
        const auto sidechainHz = params.complex ? params.sidechainHz : kStandardSidechainHz;
        const auto lowHz       = params.complex ? params.lowThruHz   : kStandardLowThruHz;
        const auto highHz      = params.complex ? params.highThruHz  : kStandardHighThruHz;

        attackPole = poleFor (attackMs, rate);
        release.setTimes (releaseMs, arcOn, rate);
        gate.setThreshold (params.gateDb);

        const auto split = bandsActive (params);

        for (auto& ch : channels)
        {
            ch.sidechain.setCutoff (sidechainHz);
            ch.bands.setCutoffs (lowHz, highHz);

            // Cleared while the split is bypassed, so that moving LOW THRU off
            // its rail starts the crossover from silence rather than from
            // whatever was in it when it was last switched out. Free: the
            // filters are not running.
            if (! split)
                ch.bands.reset();
        }
    }

    double rate = 44100.0;
    int numActiveChannels = 2;

    std::array<Channel, 2> channels;
    Gate gate;
    Limiter limiter;
    ReleaseStage release;
    Smoother amountSmoother, outputSmoother;

    float attackPole = 0.0f;
    float envelopeDb = 0.0f;

    Params params;
    float reportedReductionDb = 0.0f;
};

} // namespace bmo::vcomp

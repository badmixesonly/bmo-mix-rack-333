#pragma once

#include <cmath>

namespace bmo::sat
{

//==============================================================================
/** One-pole topology-preserving (TPT) filter, after Zavalishin.

    Two properties matter here and neither holds for a direct-form one-pole.

    Its state stays meaningful while the coefficients move, so the band split
    can follow a smoothed control without the coefficient discontinuity that
    makes a direct form click.

    And its low-pass and high-pass outputs sum to the input exactly, sample for
    sample, at every frequency. That is what makes the two-band split in
    DspCore a split rather than a filter: with the saturation switched out the
    two bands recombine to the signal that went in, so a bypass nulls and a
    partial Mix cannot comb.
*/
struct OnePole
{
    enum class Output { lowpass, highpass };

    float g = 0.0f, bigG = 0.0f, state = 0.0f;

    void setCutoff (double frequencyHz, double sampleRate) noexcept
    {
        // Prewarp, so the digital corner lands on the analogue one.
        const auto gg = std::tan (kPi * frequencyHz / sampleRate);
        g    = (float) gg;
        bigG = (float) (gg / (1.0 + gg));
    }

    void reset() noexcept { state = 0.0f; }

    /** Both outputs at once. lowpass + highpass == input, exactly. */
    void process (float input, float& lowpass, float& highpass) noexcept
    {
        const auto v = (input - state) * bigG;
        lowpass  = state + v;
        highpass = input - lowpass;
        state    = lowpass + v;
    }

    float process (Output type, float input) noexcept
    {
        float lp = 0.0f, hp = 0.0f;
        process (input, lp, hp);
        return type == Output::lowpass ? lp : hp;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
};

//==============================================================================
/** First-order shelf built from a one-pole, used in exactly invertible pairs.

        H(s) = (1 + m*s/w) / (1 + s/w)

    Unity at DC, m at high frequency. The inverse is the same structure with
    corner w/m and gain 1/m, so a pair cancels to unity wherever nothing in
    between it is misbehaving.
*/
class Shelf
{
public:
    void set (double cornerHz, float highFrequencyGain, double sampleRate) noexcept
    {
        gain = highFrequencyGain;
        pole.setCutoff (std::fmin (std::fmax (cornerHz, 1.0), sampleRate * 0.45), sampleRate);
    }

    void reset() noexcept { pole.reset(); }

    float process (float x) noexcept
    {
        float lp = 0.0f, hp = 0.0f;
        pole.process (x, lp, hp);
        return lp + gain * hp;
    }

private:
    OnePole pole;
    float   gain = 1.0f;
};

//==============================================================================
/** Topology-preserving state-variable filter, after Zavalishin, used here for
    one peaking bell and one high-pass.

    TPT rather than a direct-form biquad because its state stays meaningful
    while the coefficients move, so a control can be swept without the
    coefficient discontinuity that makes a direct form click.
*/
struct Svf
{
    float g = 0.0f, k = 1.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1eq = 0.0f, ic2eq = 0.0f;

    void set (double frequencyHz, double q, double sampleRate) noexcept
    {
        const auto gg = std::tan (kPi * std::fmin (frequencyHz, sampleRate * 0.45) / sampleRate);
        const auto kk = 1.0 / std::fmax (q, 0.05);

        g  = (float) gg;
        k  = (float) kk;
        a1 = (float) (1.0 / (1.0 + gg * (gg + kk)));
        a2 = (float) (gg * (double) a1);
        a3 = (float) (gg * (double) a2);
    }

    void reset() noexcept { ic1eq = ic2eq = 0.0f; }

    /** Band-pass, normalised to peak at unity, and high-pass, in one pass. */
    void process (float x, float& bandpass, float& highpass) noexcept
    {
        const auto v3 = x - ic2eq;
        const auto v1 = a1 * ic1eq + a2 * v3;
        const auto v2 = ic2eq + a2 * ic1eq + a3 * v3;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        bandpass = k * v1;
        highpass = x - k * v1 - v2;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
};

//==============================================================================
/** A peaking bell: the dry signal with a scaled, soft-limited band-pass added
    to it.

    y = x + threshold * tanh((gain - 1) * bandpass(x) / threshold)

    A purely linear bell -- y = x + (gain - 1) * bandpass(x) -- is what this
    was until the 0.4.0 test pass measured BMO's crest factor 0.6 dB above the
    Fuji target (22.81 vs 22.19 dB) while its energy in the same band matched
    or trailed the target. Isolating each stage of the chain (`measure_sat
    compare` against the actual Fuji before/after pair, with each generator
    zeroed in turn) found the +11 dB, Q 0.9 bell entirely responsible: silence
    it and the crest change on the real reference file flips from +2.82 dB to
    -2.38 dB, while the other stages barely move it. A high-Q boost sitting
    after the waveshaper passes sibilant transients through at full gain, so
    the same average band energy arrives peakier than the target's -- read
    back as "more sibilant" without an EQ or de-esser having anything to fix,
    which matches the test doc's finding almost exactly.

    Soft-limiting only the boost this filter adds -- never the dry signal --
    tames those transient spikes without touching the steady-state band gain
    the average-energy measurement depends on. `threshold` was fitted the same
    way as everything else in this file: swept against the real Fuji pair
    until the crest-factor change matched (+1.69 vs the target's +1.70 dB),
    which cost the band deltas at most 0.4 dB versus the unclipped version.
*/
class Bell
{
public:
    void set (double frequencyHz, double q, float gain, double sampleRate) noexcept
    {
        svf.set (frequencyHz, q, sampleRate);
        amount = gain - 1.0f;
    }

    void reset() noexcept { svf.reset(); }

    float process (float x) noexcept
    {
        float bp = 0.0f, hp = 0.0f;
        svf.process (x, bp, hp);
        const auto boost = amount * bp;
        const auto tamed = threshold * std::tanh (boost / threshold);
        return x + tamed;
    }

    /** Fitted against the real Fuji before/after pair; see the class comment. */
    static constexpr float threshold = 0.42f;

private:
    Svf   svf;
    float amount = 0.0f;
};

//==============================================================================
/** Second-order high-pass, for the bottom of the voicing. */
class HighPass
{
public:
    void set (double frequencyHz, double sampleRate) noexcept
    {
        svf.set (frequencyHz, 0.707, sampleRate);
        active = frequencyHz > 1.0;
    }

    void reset() noexcept { svf.reset(); }

    float process (float x) noexcept
    {
        if (! active)
            return x;

        float bp = 0.0f, hp = 0.0f;
        svf.process (x, bp, hp);
        return hp;
    }

private:
    Svf  svf;
    bool active = false;
};

//==============================================================================
/** DC blocker.

    An asymmetric shaper is not mean-preserving: feed it a symmetric signal and
    the output sits off zero, by an amount that moves with the drive and with
    the programme. Left in, that offset eats headroom, thumps whenever the
    drive is automated, and shows up in the crest factor as a peak that is not
    music. It is removed here rather than left for whatever is downstream.

    Deliberately far below the audio band: this is the one filter in the chain
    that is not part of the sound.
*/
class DcBlocker
{
public:
    void prepare (double sampleRate) noexcept
    {
        pole.setCutoff (kCornerHz, sampleRate);
        reset();
    }

    void reset() noexcept { pole.reset(); }

    float process (float x) noexcept { return pole.process (OnePole::Output::highpass, x); }

private:
    static constexpr double kCornerHz = 6.0;

    OnePole pole;
};

} // namespace bmo::sat

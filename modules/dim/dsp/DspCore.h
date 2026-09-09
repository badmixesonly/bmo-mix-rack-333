#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace bmo::dim
{

inline constexpr float kPi = 3.14159265358979323846f;

/** One-pole parameter smoother, same shape as the ones in modules/sat/dsp and
    modules/opto/dsp -- see those for why it snaps once inside epsilon. */
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

        if (std::abs (target - current) < 1.0e-6f)
            current = target;

        return current;
    }

    float value() const noexcept { return current; }

private:
    float coeff = 1.0f, current = 0.0f, target = 0.0f;
};

//==============================================================================
/** A crossfading delay-line pitch shifter, one voice.

    A read pointer moving at a rate other than one sample per sample is a pitch
    shift, and it is also a pointer that eventually runs into the writer. The
    standard fix, and the one here: run two taps half a window apart and
    crossfade between them with raised cosines that sum to one, so whichever
    tap is about to wrap is the one being faded out.

    The window is the whole compromise. Short windows make the crossfade
    audible as flutter; long ones smear transients. 30 ms is the usual landing
    point for a detuner at these depths.

    This costs no *reported* latency, which is worth being explicit about: the
    mid path is a wire and this voice only ever adds to the side signal, so
    there is nothing for the host to compensate. The window delay is part of
    the effect, not a delay through the module. See DimDsp::latencyForParams.
*/
class DetuneVoice
{
public:
    void prepare (double sampleRate)
    {
        window = std::max (64, (int) std::lround (sampleRate * 0.030));
        buffer.assign ((size_t) window * 2 + 4, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
        phase = 0.0f;
    }

    /** cents > 0 shifts up, < 0 down. */
    void setCents (float cents) noexcept
    {
        // A pitch ratio is 2^(cents/1200); the read pointer has to drift by the
        // difference from unity, so that is what the phase accumulates.
        const auto ratio = std::pow (2.0f, cents / 1200.0f);
        phaseInc = (1.0f - ratio) / (float) std::max (window, 1);
    }

    float process (float x) noexcept
    {
        if (buffer.empty())
            return 0.0f;

        const auto len = (int) buffer.size();

        buffer[(size_t) writeIdx] = x;

        // Two taps, half a window apart, each faded by a raised cosine. The two
        // windows sum to exactly one, so a steady input comes out steady.
        auto tap = [this, len] (float ph) noexcept
        {
            const auto delay = ph * (float) window;
            const auto rd    = (float) writeIdx - delay;

            auto i0 = (int) std::floor (rd);
            const auto frac = rd - (float) i0;

            i0 %= len; if (i0 < 0) i0 += len;
            auto i1 = i0 + 1; if (i1 >= len) i1 -= len;

            return buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
        };

        const auto ph2 = phase >= 0.5f ? phase - 0.5f : phase + 0.5f;
        const auto g1  = 0.5f * (1.0f - std::cos (2.0f * kPi * phase));
        const auto g2  = 0.5f * (1.0f - std::cos (2.0f * kPi * ph2));

        const auto out = g1 * tap (phase) + g2 * tap (ph2);

        phase += phaseInc;
        while (phase >= 1.0f) phase -= 1.0f;
        while (phase <  0.0f) phase += 1.0f;

        if (++writeIdx >= len)
            writeIdx = 0;

        return out;
    }

private:
    std::vector<float> buffer;
    int   window = 0, writeIdx = 0;
    float phase = 0.0f, phaseInc = 0.0f;
};

//==============================================================================
/** A cascade of first-order all-passes, coefficient swept from outside.

    Magnitude flat by construction: each stage only rotates phase. Sweeping the
    coefficient is what makes this a phaser rather than a fixed decorrelator,
    which is why BMO Dimension has no separate phaser stage -- they are the
    same object with the LFO connected.

    Six stages decorrelates audibly without the sweep turning into a comb
    whistle at high depth.
*/
class AllPassChain
{
public:
    static constexpr int kStages = 6;

    void reset() noexcept { state.fill (0.0f); }

    float process (float x, float a) noexcept
    {
        a = std::clamp (a, -0.95f, 0.95f);

        for (int i = 0; i < kStages; ++i)
        {
            const auto y = -a * x + state[(size_t) i];
            state[(size_t) i] = x + a * y;
            x = y;
        }

        return x;
    }

private:
    std::array<float, (size_t) kStages> state {};
};

//==============================================================================
/** Gerzon's bass shuffler, as a complementary one-pole split.

    The low band of the side signal is scaled and the high band is not, because
    the ears hear stereo as narrower in the bass than in the treble and the
    shuffler is the correction for it. Low and high sum back to the input
    exactly, so at unity this stage is a wire rather than an approximation of
    one -- which is what "fully phase compensated" has to mean if it is going
    to be asserted rather than hoped for.
*/
class Shuffler
{
public:
    void prepare (double sr) noexcept { sampleRate = std::max (sr, 1.0); reset(); }
    void reset() noexcept { z = 0.0f; }

    void setFrequency (float hz) noexcept
    {
        const auto f = std::clamp (hz, 20.0f, (float) (sampleRate * 0.45));
        coeff = 1.0f - std::exp (-2.0f * kPi * f / (float) sampleRate);
    }

    float process (float s, float amount) noexcept
    {
        z += coeff * (s - z);
        const auto high = s - z;
        return z * amount + high;
    }

private:
    double sampleRate = 44100.0;
    float  coeff = 0.1f, z = 0.0f;
};

//==============================================================================
/** BMO Dimension: split to mid/side, work on the side, sum back.

    Three stages in series on S -- generate, diffuse, image -- and a mid path
    that is a plain wire. Because L + R = 2M, anything done to S alone is
    invisible in the mono sum, so width, shuffle and diffuse cannot damage mono
    compatibility however they are set.

    **Rotation and asymmetry are the two exceptions, and they are deliberate.**
    Rotation turns the whole soundfield, which necessarily moves centre material
    off centre and therefore changes the mono sum; asymmetry skews left against
    right, and the S1's own manual says outright that it acts in mono as well as
    in stereo. Both are identity at their defaults, so a Dimension left alone is
    still mono-exact -- but a claim that the module is unconditionally mono-safe
    would be wrong once either is turned, and it is worth stating accurately
    rather than broadly.
*/
class DspCore
{
public:
    struct Params
    {
        float widthPercent     = 100.0f;
        float shuffleAmount    = 1.0f;
        float shuffleFreqHz    = 700.0f;
        float detuneCents      = 10.0f;
        bool  detuneOn         = false;
        float diffusePercent   = 0.0f;
        float rateHz           = 0.40f;
        float depthPercent     = 50.0f;
        float rotationDegrees  = 0.0f;
        float asymmetryPercent = 0.0f;
    };

    void prepare (double sr, int, int)
    {
        sampleRate = std::max (sr, 1.0);

        up.prepare (sampleRate);
        down.prepare (sampleRate);
        shuffler.prepare (sampleRate);

        // 8 ms on the continuous controls: fast enough that a knob move feels
        // immediate, slow enough that an automated width sweep does not step.
        widthSm.prepare (sampleRate, 8.0);
        shuffleSm.prepare (sampleRate, 8.0);
        diffuseSm.prepare (sampleRate, 8.0);
        depthSm.prepare (sampleRate, 8.0);
        rotSm.prepare (sampleRate, 8.0);
        asymSm.prepare (sampleRate, 8.0);

        reset();
    }

    void reset()
    {
        up.reset();
        down.reset();
        chain.reset();
        shuffler.reset();
        lfoPhase = 0.0f;
        primed = false;
    }

    void setParams (const Params& p)
    {
        params = p;

        widthSm  .setTarget (p.widthPercent * 0.01f);
        shuffleSm.setTarget (p.shuffleAmount);
        diffuseSm.setTarget (p.diffusePercent * 0.01f);
        depthSm  .setTarget (p.depthPercent * 0.01f);
        rotSm    .setTarget (p.rotationDegrees * kPi / 180.0f);
        asymSm   .setTarget (std::clamp (p.asymmetryPercent * 0.01f, -1.0f, 1.0f));

        // The two voices are opposed, so the pair sums back toward the centre
        // rather than pulling the whole image one way.
        up  .setCents ( p.detuneCents);
        down.setCents (-p.detuneCents);

        shuffler.setFrequency (p.shuffleFreqHz);

        lfoInc = (float) (std::max (p.rateHz, 0.0f) / sampleRate);

        if (! primed)
        {
            widthSm.snap (p.widthPercent * 0.01f);
            shuffleSm.snap (p.shuffleAmount);
            diffuseSm.snap (p.diffusePercent * 0.01f);
            depthSm.snap (p.depthPercent * 0.01f);
            rotSm.snap (p.rotationDegrees * kPi / 180.0f);
            asymSm.snap (std::clamp (p.asymmetryPercent * 0.01f, -1.0f, 1.0f));
            primed = true;
        }
    }

    void process (float* const* channels, int numChannels, int numSamples)
    {
        if (numChannels < 1)
            return;

        auto* l = channels[0];
        auto* r = numChannels > 1 ? channels[1] : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto inL = l[i];
            const auto inR = r != nullptr ? r[i] : inL;

            auto mid  = 0.5f * (inL + inR);
            auto side = 0.5f * (inL - inR);

            // -- Generate ----------------------------------------------------
            // Detune reads the mid, because on a mono source that is the only
            // thing there. The two shifted voices differenced give side content
            // that did not exist a sample ago.
            if (params.detuneOn)
            {
                const auto a = up.process (mid);
                const auto b = down.process (mid);
                side += 0.5f * (a - b);
            }

            // -- Diffuse -----------------------------------------------------
            const auto diffuse = diffuseSm.tick();
            const auto depth   = depthSm.tick();

            if (diffuse > 0.0f)
            {
                const auto lfo = std::sin (2.0f * kPi * lfoPhase);
                const auto a   = 0.55f + 0.40f * depth * lfo;
                const auto wet = chain.process (side, a);
                side += diffuse * (wet - side);
            }

            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f)
                lfoPhase -= 1.0f;

            // -- Image -------------------------------------------------------
            side = shuffler.process (side, shuffleSm.tick());
            side *= widthSm.tick();

            // Rotation mixes mid and side, which is exactly what turning a
            // soundfield does -- and the one place the mono sum stops being 2M.
            // Identity at zero.
            const auto theta = rotSm.tick();

            if (theta != 0.0f)
            {
                const auto c = std::cos (theta), s = std::sin (theta);
                const auto m2 = mid * c - side * s;
                const auto s2 = mid * s + side * c;
                mid = m2; side = s2;
            }

            auto outL = mid + side;
            auto outR = mid - side;

            // Asymmetry as unequal trim either side of centre. Gerzon's exact
            // law is not published in the S1's manual -- it says only that the
            // control changes the left/right balance in stereo and in mono
            // without moving centre sounds. This is the reading that stays
            // linear and is identity at zero; it wants a listening check
            // against a reference before 1.0.
            const auto asym = asymSm.tick();

            if (asym != 0.0f)
            {
                outL *= 1.0f + asym;
                outR *= 1.0f - asym;
            }

            l[i] = outL;

            if (r != nullptr)
                r[i] = outR;
        }
    }

private:
    double sampleRate = 44100.0;

    Params       params;
    DetuneVoice  up, down;
    AllPassChain chain;
    Shuffler     shuffler;

    Smoother widthSm, shuffleSm, diffuseSm, depthSm, rotSm, asymSm;

    float lfoPhase = 0.0f, lfoInc = 0.0f;
    bool  primed = false;
};

} // namespace bmo::dim

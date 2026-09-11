#pragma once

#include "modules/tune/dsp/Pitch.h"
#include <cmath>
#include <vector>

namespace bmo::tune
{

/** Polyphase Kaiser-windowed sinc for fractional reads (spec §5.5).

    `Taps` coefficients per phase, `kPhases` phases, linear interpolation
    between adjacent phases -- 256 phases alone leave a time quantisation of
    1/256 sample, which at 0.4 fs is a -40 dB error; interpolating between
    rows pushes that below the kernel's own error.

    Built once at prepare time (it allocates); read on the audio thread.

    `cutoff` is a fraction of Nyquist. At 1.0 with a whole-sample position the
    kernel is a pure delay -- every other tap lands on a zero of the sinc --
    which is what lets the engines pass audio bit-exactly while they are not
    correcting. Below 1.0 it is also the anti-aliasing filter spec §5.4 asks
    for when the read runs faster than the write: content above fs / (2 rho)
    is folded back by a read at rate rho, so it has to be gone first.
*/
template <int Taps>
class SincTable
{
public:
    static_assert (Taps % 2 == 0, "an even tap count centres the kernel between samples");

    static constexpr int kTaps = Taps;
    static constexpr int kHalf = Taps / 2;
    static constexpr int kPhases = 256;

    /** Whole samples the newest input must be ahead of a read position. */
    static constexpr int kLookahead = kHalf;

    void build (double cutoff, double beta)
    {
        table.assign ((size_t) (kPhases + 1) * kTaps, 0.0f);
        const auto i0Beta = besselI0 (beta);

        for (int p = 0; p <= kPhases; ++p)
        {
            const auto frac = (double) p / kPhases;
            double row[Taps];
            double sum = 0.0;

            for (int k = 0; k < Taps; ++k)
            {
                // Tap k reads sample (i - kHalf + 1 + k); its distance from
                // the read position i + frac is t.
                const auto t = frac + (kHalf - 1 - k);
                const auto x = cutoff * t;

                // sin(pi n) in double is ~1e-16 n, not 0, so without this the
                // "zero" taps of a whole-sample read are 1e-17 and the read is
                // not an exact copy -- which the passthrough test caught.
                const auto nearestInteger = std::round (x);
                const auto onZero = nearestInteger != 0.0 && std::abs (x - nearestInteger) < 1.0e-12;
                const auto sinc = std::abs (x) < 1.0e-12 ? 1.0
                                : onZero ? 0.0 : std::sin (kPi * x) / (kPi * x);
                const auto r = t / kHalf;
                const auto w = std::abs (r) < 1.0 ? besselI0 (beta * std::sqrt (1.0 - r * r)) / i0Beta : 0.0;

                row[k] = cutoff * sinc * w;
                sum += row[k];
            }

            // Unity at DC on every phase, so a fractional read never ripples
            // the level as the phase moves.
            for (int k = 0; k < Taps; ++k)
                table[(size_t) (p * Taps + k)] = (float) (row[k] / sum);
        }
    }

    /** Reads `ring` (power-of-two size, `mask`) at fractional index
        `position`. The caller guarantees position + kHalf has been written. */
    float read (const float* ring, int mask, double position) const noexcept
    {
        const auto whole = std::floor (position);
        const auto frac = position - whole;
        const auto i = (int) (long long) whole;

        const auto phase = frac * kPhases;
        const auto p = std::min ((int) phase, kPhases - 1);
        const auto mix = (float) (phase - p);

        const float* a = table.data() + (size_t) p * Taps;
        const float* b = a + Taps;
        const auto base = i - kHalf + 1;

        float accA = 0.0f, accB = 0.0f;

        for (int k = 0; k < Taps; ++k)
        {
            const auto x = ring[(size_t) ((base + k) & mask)];
            accA += a[k] * x;
            accB += b[k] * x;
        }

        return accA + mix * (accB - accA);
    }

private:
    static double besselI0 (double x)
    {
        // Power series; converges fast for the betas a window uses.
        double sum = 1.0, term = 1.0;
        const auto half = 0.5 * x;

        for (int k = 1; k < 64; ++k)
        {
            term *= (half / k) * (half / k);
            sum += term;
            if (term < 1.0e-17 * sum)
                break;
        }

        return sum;
    }

    std::vector<float> table;
};

} // namespace bmo::tune

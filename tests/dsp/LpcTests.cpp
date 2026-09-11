/*
    Linear prediction and line spectral frequencies (spec §6.2, T-4).

    The claims: Levinson recovers a known all-pole process; it never returns
    an unstable filter, including on a rank-deficient autocorrelation; LSFs
    round-trip, stay ordered, stay stable under interpolation; and scaling
    them moves a formant by exactly the ratio asked for.
*/

#include "modules/tune/dsp/Lpc.h"
#include "tests/TestUtil.h"
#include "tools/common/Signals.h"

#include <complex>
#include <string>
#include <vector>

using namespace bmo::tune;
using namespace bmo::tune::test;
namespace sig = bmo::tune::signals;

namespace
{
    /** |1 / A(e^jw)| in dB. */
    double responseDb (const lpc::Coefficients& a, int order, double w)
    {
        std::complex<double> s = 0.0;
        for (int i = 0; i <= order; ++i)
            s += a[(size_t) i] * std::polar (1.0, -w * i);
        return -20.0 * std::log10 (std::abs (s));
    }

    /** A random stable filter from random reflection coefficients. */
    lpc::Coefficients randomStable (int order, sig::Random& rng, double maxK = 0.95)
    {
        lpc::Coefficients a {}, prev {};
        a[0] = 1.0;
        for (int m = 1; m <= order; ++m)
        {
            const auto k = maxK * rng.uniform();
            prev = a;
            for (int i = 1; i < m; ++i)
                a[(size_t) i] = prev[(size_t) i] + k * prev[(size_t) (m - i)];
            a[(size_t) m] = k;
        }
        return a;
    }
}

int main()
{
    //== Levinson recovers a known AR(4) =======================================
    {
        // Two resonances, poles at radius 0.95 and 0.9.
        const double r1 = 0.95, w1 = 0.3, r2 = 0.9, w2 = 1.1;
        const double c1[3] = { 1.0, -2.0 * r1 * std::cos (w1), r1 * r1 };
        const double c2[3] = { 1.0, -2.0 * r2 * std::cos (w2), r2 * r2 };
        double truth[5] = { 0, 0, 0, 0, 0 };
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                truth[i + j] += c1[i] * c2[j];

        sig::Random rng (8);
        std::vector<float> x (200000);
        double y[5] = {};
        for (auto& v : x)
        {
            double out = rng.gaussian();
            for (int i = 1; i <= 4; ++i)
                out -= truth[i] * y[i];
            for (int i = 4; i > 1; --i)
                y[i] = y[i - 1];
            y[1] = out;
            v = (float) out;
        }

        double r[5];
        lpc::autocorrelate (x.data(), (int) x.size(), 4, r);
        lpc::Coefficients a;
        const auto err = lpc::levinson (r, 4, a);

        double worst = 0.0;
        for (int i = 1; i <= 4; ++i)
            worst = std::max (worst, std::abs (a[(size_t) i] - truth[i]));

        report ("AR(4) coefficient error, 200k samples", worst);
        check (err > 0.0 && worst < 0.02, "Levinson recovers a known AR(4) process");
    }

    //== Rank-deficient input ==================================================
    {
        // A pure tone's autocorrelation is rank 2: Levinson at order 24 on it
        // raw is on the edge of instability. Conditioned, it must not be.
        std::vector<float> tone (2048);
        for (size_t i = 0; i < tone.size(); ++i)
            tone[i] = (float) std::sin (0.1 * (double) i);

        double r[25];
        lpc::autocorrelate (tone.data(), (int) tone.size(), 24, r);
        lpc::condition (r, 24, 48000.0);
        lpc::Coefficients a;
        const auto err = lpc::levinson (r, 24, a);
        report ("pure tone, order 24, conditioned: largest |k|", lpc::largestReflection (a, 24));
        check (err > 0.0 && lpc::largestReflection (a, 24) < 1.0, "a conditioned pure tone gives a stable order-24 filter");

        double silent[25] = {};
        check (lpc::levinson (silent, 24, a) < 0.0 && a[0] == 1.0 && a[1] == 0.0,
               "silence is refused and leaves the identity filter");
    }

    //== Every frame of the synthetic voices is stable =========================
    {
        int frames = 0, unstable = 0, refused = 0, noLsf = 0;
        for (auto hz : { 82.41, 130.8, 220.0, 440.0, 880.0 })
            for (auto rate : { 44100.0, 48000.0, 96000.0 })
            {
                const auto x = sig::voice (sig::vibrato (hz, 50.0, 5.5, 1.0, rate), rate).samples;
                const auto n = (int) (0.025 * rate);
                const auto hop = (int) (0.005 * rate);
                for (int at = 0; at + n <= (int) x.size(); at += hop)
                {
                    double r[25];
                    lpc::autocorrelate (x.data() + at, n, 24, r);
                    lpc::condition (r, 24, rate);
                    lpc::Coefficients a;
                    ++frames;
                    if (lpc::levinson (r, 24, a) < 0.0) { ++refused; continue; }
                    if (lpc::largestReflection (a, 24) >= 1.0) ++unstable;

                    lpc::Lsf lsf {};
                    if (! lpc::toLsf (a, 24, lsf)) ++noLsf;
                }
            }

        report ("voice frames analysed", frames);
        report ("refused", refused);
        report ("frames toLsf could not convert", noLsf);
        check (unstable == 0, "every accepted voice frame has all poles inside the unit circle (T-4)");
        check (refused == 0, "and none is refused");
        check (noLsf == 0, "and every one converts to LSF -- or the formant stage silently falls back to identity");
    }

    //== LSF round trip, ordering, interpolation ===============================
    {
        sig::Random rng (99);
        double worstTrip = 0.0;
        int lsfFailures = 0, unstable = 0;

        for (int trial = 0; trial < 500; ++trial)
        {
            const auto order = 2 * (1 + (int) (rng.next() % 12));   // 2 .. 24
            const auto a = randomStable (order, rng);
            lpc::Lsf lsf {};
            if (! lpc::toLsf (a, order, lsf)) { ++lsfFailures; continue; }

            lpc::Coefficients back;
            lpc::fromLsf (lsf, order, back);
            for (int i = 0; i <= order; ++i)
                worstTrip = std::max (worstTrip, std::abs (back[(size_t) i] - a[(size_t) i]));

            // Interpolate toward another random filter of the same order.
            const auto b = randomStable (order, rng);
            lpc::Lsf lsfB {};
            if (! lpc::toLsf (b, order, lsfB)) { ++lsfFailures; continue; }

            for (double t = 0.0; t <= 1.0; t += 0.125)
            {
                lpc::Lsf mid {};
                lpc::interpolate (lsf, lsfB, t, order, mid);
                lpc::Coefficients m;
                lpc::fromLsf (mid, order, m);
                if (lpc::largestReflection (m, order) >= 1.0) ++unstable;
            }
        }

        report ("LSF round-trip worst coefficient error", worstTrip);
        check (lsfFailures == 0, "toLsf succeeds on every stable filter of order 2..24");
        check (worstTrip < 1.0e-6, "LPC -> LSF -> LPC round-trips");
        check (unstable == 0, "every LSF interpolation between stable filters is stable (T-4)");
    }

    //== Formant shift by LSF scaling ==========================================
    {
        // One resonance at 1 kHz, 48 kHz, order 8 (a bit of tilt around it).
        const double fs = 48000.0;
        const double w = 2.0 * kPi * 1000.0 / fs, rad = 0.97;
        lpc::Coefficients a {};
        a[0] = 1.0; a[1] = -2.0 * rad * std::cos (w); a[2] = rad * rad;
        const int order = 8;

        lpc::Lsf lsf {};
        check (lpc::toLsf (a, order, lsf), "an order-8 filter with a single resonance converts to LSF");

        const auto peakHz = [&] (const lpc::Coefficients& c)
        {
            double best = -1e9, bestHz = 0.0;
            for (double f = 200.0; f < 4000.0; f += 0.5)
            {
                const auto db = responseDb (c, order, 2.0 * kPi * f / fs);
                if (db > best) { best = db; bestHz = f; }
            }
            return bestHz;
        };

        for (auto cents : { -300.0, 100.0, 400.0 })
        {
            auto shifted = lsf;
            lpc::shiftFormants (shifted, order, std::exp2 (cents / 1200.0));
            lpc::Coefficients s;
            lpc::fromLsf (shifted, order, s);
            const auto moved = 1200.0 * std::log2 (peakHz (s) / peakHz (a));
            char buf[96];
            std::snprintf (buf, sizeof buf, "formant shift %+.0f c: resonance moved", cents);
            report (buf, moved, "c");
            // LSF scaling is not a perfect warp -- the LSFs that carry no
            // resonance move too and tilt the envelope a little -- so the
            // resonance lands within a few percent of the shift, not exactly.
            check (std::abs (moved - cents) < 0.05 * std::abs (cents) + 5.0, std::string ("LSF scaling moves a resonance by its ratio, within 5 %: ") + std::to_string ((int) cents));
            check (lpc::largestReflection (s, order) < 1.0, "and leaves it stable");
        }
    }

    return finish ("lpc");
}

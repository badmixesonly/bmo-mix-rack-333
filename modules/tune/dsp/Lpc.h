#pragma once

#include "modules/tune/dsp/Pitch.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace bmo::tune::lpc
{

/** Linear prediction, for HYBRID's formant stage (spec §6.2).

    Everything is fixed-size and allocation-free: an envelope is estimated a
    couple of hundred times a second on the audio thread. Orders are even --
    the line-spectral-frequency form below needs it -- and at most kMaxOrder.

    The pipeline an engine runs:

        autocorrelate    a Hann-windowed frame, lags 0..p
        condition        Gaussian lag window + white-noise correction, so a
                         near-periodic frame (a pure tone is rank-deficient)
                         still gives a positive-definite matrix
        levinson         -> A(z), reflection coefficients, prediction error;
                         every |k| < 1 or the frame is refused
        toLsf / fromLsf  the line spectral frequencies, which is the domain
                         the spec says to interpolate in (never raw a_i) and
                         the one where a formant shift is a rescaling
*/

inline constexpr int kMaxOrder = 32;

using Coefficients = std::array<double, kMaxOrder + 1>;   // a[0] = 1
using Lsf = std::array<double, kMaxOrder>;                 // radians, ascending, in (0, pi)

/** r[0..order] of x[0..n), with a Hann window applied on the fly. */
inline void autocorrelate (const float* x, int n, int order, double* r) noexcept
{
    // Window once into a small stack buffer would be faster; the frames are
    // at most a few thousand samples, so window per product instead and keep
    // the function allocation-free with no size limit.
    const auto w = [n] (int i) { return 0.5 - 0.5 * std::cos (2.0 * kPi * (i + 0.5) / n); };

    for (int k = 0; k <= order; ++k)
    {
        double s = 0.0;
        for (int i = k; i < n; ++i)
            s += (double) x[i] * w (i) * (double) x[i - k] * w (i - k);
        r[k] = s;
    }
}

/** Lag window (Gaussian, `bandwidthHz` wide) and white-noise correction:
    r[0] *= 1 + whiteNoise. The spec's r[0] *= 1.0001 is whiteNoise = 1e-4. */
inline void condition (double* r, int order, double sampleRate,
                       double bandwidthHz = 60.0, double whiteNoise = 1.0e-4) noexcept
{
    r[0] *= 1.0 + whiteNoise;

    for (int k = 1; k <= order; ++k)
    {
        const auto x = 2.0 * kPi * bandwidthHz * k / sampleRate;
        r[k] *= std::exp (-0.5 * x * x);
    }
}

/** Levinson-Durbin. Writes a[0..order] (a[0] = 1) and returns the final
    prediction error, or a negative number if the recursion went unstable
    (some |k| >= 1: the autocorrelation was not positive definite) or the
    frame was silent. On failure `a` is left as the identity filter. */
inline double levinson (const double* r, int order, Coefficients& a, double* reflection = nullptr) noexcept
{
    a.fill (0.0);
    a[0] = 1.0;

    if (! (r[0] > 1.0e-20) || ! std::isfinite (r[0]))
        return -1.0;

    double error = r[0];
    Coefficients previous {};

    for (int m = 1; m <= order; ++m)
    {
        double acc = r[m];
        for (int i = 1; i < m; ++i)
            acc += a[(size_t) i] * r[m - i];

        const auto k = -acc / error;

        if (! std::isfinite (k) || std::abs (k) >= 1.0)
        {
            a.fill (0.0);
            a[0] = 1.0;
            return -1.0;
        }

        if (reflection)
            reflection[m - 1] = k;

        previous = a;
        for (int i = 1; i < m; ++i)
            a[(size_t) i] = previous[(size_t) i] + k * previous[(size_t) (m - i)];
        a[(size_t) m] = k;

        error *= 1.0 - k * k;
    }

    return error;
}

namespace detail
{
    /** F(w) for a symmetric polynomial c[0..deg] (deg even) on the unit
        circle, with the linear phase removed:
        F(w) = c[deg/2] + 2 sum_{i < deg/2} c[i] cos((deg/2 - i) w). */
    inline double symmetricOnCircle (const double* c, int deg, double w) noexcept
    {
        const auto half = deg / 2;
        double s = c[half];
        for (int i = 0; i < half; ++i)
            s += 2.0 * c[i] * std::cos ((half - i) * w);
        return s;
    }

    /** The same F, as a Chebyshev series in x = cos w, by Clenshaw's
        recurrence: no cosines per point, so a fine grid is affordable. */
    inline double chebyshev (const double* c, int deg, double x) noexcept
    {
        const auto half = deg / 2;
        double b1 = 0.0, b2 = 0.0;

        // d_0 = c[half], d_m = 2 c[half - m].
        for (int m = half; m >= 1; --m)
        {
            const auto b0 = 2.0 * c[half - m] + 2.0 * x * b1 - b2;
            b2 = b1;
            b1 = b0;
        }

        return c[half] + x * b1 - b2;
    }

    template <int Grid>
    const std::array<double, Grid + 1>& cosineGrid() noexcept
    {
        static const std::array<double, Grid + 1> table = []
        {
            std::array<double, Grid + 1> t {};
            for (int g = 0; g <= Grid; ++g)
                t[(size_t) g] = std::cos (kPi * (double) g / Grid);
            return t;
        }();
        return table;
    }

    /** The roots of F in (0, pi), found on a grid of `Grid` cells and refined
        by bisection in w. */
    template <int Grid>
    int rootsOnGrid (const double* c, int deg, double* roots, int maxRoots) noexcept
    {
        const auto& xs = cosineGrid<Grid>();
        int found = 0;
        double fPrev = chebyshev (c, deg, xs[0]);

        for (int g = 1; g <= Grid && found < maxRoots; ++g)
        {
            const auto f = chebyshev (c, deg, xs[(size_t) g]);

            if ((fPrev <= 0.0) != (f <= 0.0))
            {
                double lo = kPi * (g - 1) / Grid, hi = kPi * g / Grid, flo = fPrev;
                for (int it = 0; it < 48; ++it)
                {
                    const auto mid = 0.5 * (lo + hi);
                    const auto fm = chebyshev (c, deg, std::cos (mid));
                    if ((flo <= 0.0) == (fm <= 0.0)) { lo = mid; flo = fm; }
                    else hi = mid;
                }
                roots[found++] = 0.5 * (lo + hi);
            }

            fPrev = f;
        }

        return found;
    }

    /** A 1024-cell grid is fine for any conditioned speech frame -- the lag
        window keeps bandwidths above ~60 Hz, which keeps LSF pairs several
        cells apart. A sharper filter can put two roots in one cell and lose
        both; then it is retried once on a grid eight times finer. */
    inline int rootsOnCircle (const double* c, int deg, double* roots, int maxRoots) noexcept
    {
        const auto found = rootsOnGrid<1024> (c, deg, roots, maxRoots);
        return found == maxRoots ? found : rootsOnGrid<8192> (c, deg, roots, maxRoots);
    }
}

/** A(z) -> LSF. Returns false (leaving lsf alone) if the roots do not come
    out as order/2 of each kind -- which for a minimum-phase A never happens,
    so false means the frame was bad and the previous envelope should stand.

    P(z) = A(z) + z^-(p+1) A(1/z) is symmetric with a trivial root at z = -1,
    Q(z) the antisymmetric one with a trivial root at z = +1. Deflated, each
    is a symmetric polynomial of degree p whose p/2 roots on the unit circle
    interleave with the other's; together they are the LSFs. */
inline bool toLsf (const Coefficients& a, int order, Lsf& lsf) noexcept
{
    if (order <= 0 || order > kMaxOrder || (order & 1))
        return false;

    double P[kMaxOrder + 2], Q[kMaxOrder + 2];
    for (int i = 0; i <= order + 1; ++i)
    {
        const auto ai = i <= order ? a[(size_t) i] : 0.0;
        const auto aj = (order + 1 - i) <= order ? a[(size_t) (order + 1 - i)] : 0.0;
        P[i] = ai + aj;
        Q[i] = ai - aj;
    }

    // Deflate: P / (1 + z^-1), Q / (1 - z^-1).
    double p[kMaxOrder + 1], q[kMaxOrder + 1];
    p[0] = P[0];
    q[0] = Q[0];
    for (int i = 1; i <= order; ++i)
    {
        p[i] = P[i] - p[i - 1];
        q[i] = Q[i] + q[i - 1];
    }

    double rp[kMaxOrder], rq[kMaxOrder];
    const auto half = order / 2;
    if (detail::rootsOnCircle (p, order, rp, half) != half || detail::rootsOnCircle (q, order, rq, half) != half)
        return false;

    // Interleave: for a minimum-phase A the lowest root is P's.
    for (int i = 0; i < half; ++i)
    {
        lsf[(size_t) (2 * i)] = rp[i];
        lsf[(size_t) (2 * i + 1)] = rq[i];
    }

    for (int i = 1; i < order; ++i)
        if (! (lsf[(size_t) i] > lsf[(size_t) i - 1]))
            return false;

    return true;
}

/** LSF -> A(z). Even indices are P's roots, odd are Q's, as toLsf writes. */
inline void fromLsf (const Lsf& lsf, int order, Coefficients& a) noexcept
{
    double p[kMaxOrder + 2] {}, q[kMaxOrder + 2] {};
    p[0] = q[0] = 1.0;
    int degree = 0;

    // Multiply out prod (1 - 2 cos w z^-1 + z^-2) for each set.
    for (int k = 0; k < order / 2; ++k)
    {
        const auto cp = -2.0 * std::cos (lsf[(size_t) (2 * k)]);
        const auto cq = -2.0 * std::cos (lsf[(size_t) (2 * k + 1)]);

        for (int i = degree + 2; i >= 2; --i)
        {
            p[i] += cp * p[i - 1] + p[i - 2];
            q[i] += cq * q[i - 1] + q[i - 2];
        }
        p[1] += cp * p[0];
        q[1] += cq * q[0];
        degree += 2;
    }

    // Re-inflate: P = P'(1 + z^-1), Q = Q'(1 - z^-1); A = (P + Q) / 2.
    a.fill (0.0);
    for (int i = 0; i <= order; ++i)
    {
        const auto P = p[i] + (i > 0 ? p[i - 1] : 0.0);
        const auto Q = q[i] - (i > 0 ? q[i - 1] : 0.0);
        a[(size_t) i] = 0.5 * (P + Q);
    }
    a[0] = 1.0;
}

/** Keeps LSFs strictly ascending, at least `minGap` apart and clear of 0 and
    pi -- which is what keeps 1/A(z) stable after an interpolation or a
    rescale. */
inline void enforceOrdering (Lsf& lsf, int order, double minGap = 0.01) noexcept
{
    lsf[0] = std::max (lsf[0], minGap);

    for (int i = 1; i < order; ++i)
        lsf[(size_t) i] = std::max (lsf[(size_t) i], lsf[(size_t) i - 1] + minGap);

    const auto top = kPi - minGap;
    if (lsf[(size_t) order - 1] > top)
    {
        lsf[(size_t) order - 1] = top;
        for (int i = order - 2; i >= 0; --i)
            lsf[(size_t) i] = std::min (lsf[(size_t) i], lsf[(size_t) i + 1] - minGap);
    }
}

/** Linear interpolation in the LSF domain, then re-ordered. */
inline void interpolate (const Lsf& a, const Lsf& b, double t, int order, Lsf& out) noexcept
{
    for (int i = 0; i < order; ++i)
        out[(size_t) i] = a[(size_t) i] + t * (b[(size_t) i] - a[(size_t) i]);

    enforceOrdering (out, order);
}

/** A formant shift of `ratio`, as a piecewise-linear frequency warp: LSFs
    below `knee` (radians) are scaled by the ratio, and those above are mapped
    linearly onto what is left of [knee * ratio, pi], so both ends stay put.

    Scaling every LSF, the obvious form, was the first one written. At order
    24 the top LSFs model the high-frequency tilt and sit within a few
    hundredths of pi; scaled up by 12 % they were pushed against pi, squeezed
    to the minimum gap, and became near-zero-bandwidth resonances at Nyquist
    that the synthesis filter's guard kept resetting -- measured as a uniform
    14 dB drop with the formants not moved at all. */
inline void shiftFormants (Lsf& lsf, int order, double ratio, double knee = 0.0) noexcept
{
    if (knee <= 0.0)
        knee = 0.2 * kPi;

    // The knee must land inside the band after scaling, with room above it.
    knee = std::min (knee, 0.85 * kPi / std::max (ratio, 1.0e-3));
    const auto kneeOut = knee * ratio;

    for (int i = 0; i < order; ++i)
    {
        auto& w = lsf[(size_t) i];
        w = w <= knee ? w * ratio
                      : kneeOut + (kPi - kneeOut) * (w - knee) / (kPi - knee);
    }

    enforceOrdering (lsf, order);
}

/** Largest pole radius of 1/A(z), by the reflection coefficients of a
    step-down recursion: every |k| < 1 <=> all roots inside the unit circle.
    Returns the largest |k| (so < 1 means stable). */
inline double largestReflection (const Coefficients& a, int order) noexcept
{
    Coefficients c = a, next {};
    double worst = 0.0;

    for (int m = order; m >= 1; --m)
    {
        const auto k = c[(size_t) m];
        worst = std::max (worst, std::abs (k));
        if (std::abs (k) >= 1.0)
            return worst;

        const auto d = 1.0 - k * k;
        for (int i = 1; i < m; ++i)
            next[(size_t) i] = (c[(size_t) i] - k * c[(size_t) (m - i)]) / d;
        for (int i = 1; i < m; ++i)
            c[(size_t) i] = next[(size_t) i];
    }

    return worst;
}

} // namespace bmo::tune::lpc

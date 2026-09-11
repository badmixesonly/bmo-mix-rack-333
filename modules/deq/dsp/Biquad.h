#pragma once

#include <cmath>
#include <complex>

namespace bmo::deq
{

inline constexpr double kPi = 3.14159265358979323846;

/** A second-order transfer function, normalised so a0 = 1:

        H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)

    This is the *description* of a band, not the structure that runs it. The
    audio path is a TPT state-variable filter (Svf.h) loaded from one of these,
    because an SVF's state stays meaningful while its coefficients move and a
    direct-form biquad's does not. Keeping the design in biquad form lets the
    tests evaluate it analytically and compare it against the running filter,
    which is the check that the two have not drifted apart.
*/
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    /** Response at a normalised angular frequency, radians per sample. */
    std::complex<double> responseAt (double w) const noexcept
    {
        const auto z1 = std::polar (1.0, -w);
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }

    double magnitudeDbAt (double hz, double sampleRate) const noexcept
    {
        const auto m = std::abs (responseAt (2.0 * kPi * hz / sampleRate));
        return 20.0 * std::log10 (m > 1.0e-300 ? m : 1.0e-300);
    }

    /** Both poles strictly inside the unit circle (the stability triangle). */
    bool isStable() const noexcept
    {
        return std::abs (a2) < 1.0 && std::abs (a1) < 1.0 + a2;
    }

    /** Both zeros inside or on the unit circle, so 1/H is realisable. */
    bool isMinimumPhase (double tolerance = 1.0e-9) const noexcept
    {
        if (b0 == 0.0)
            return false;

        const auto disc = std::sqrt (std::complex<double> (b1 * b1 - 4.0 * b0 * b2, 0.0));
        const auto r1 = std::abs ((-b1 + disc) / (2.0 * b0));
        const auto r2 = std::abs ((-b1 - disc) / (2.0 * b0));
        return r1 <= 1.0 + tolerance && r2 <= 1.0 + tolerance;
    }

    /** 1/H. The poles of the result are the zeros of this one, so it is only
        stable if this is strictly minimum phase -- check isStable() on it. */
    Biquad reciprocal() const noexcept
    {
        const auto s = 1.0 / b0;
        return { s, a1 * s, a2 * s, b1 * s, b2 * s };
    }

    bool isFinite() const noexcept
    {
        return std::isfinite (b0) && std::isfinite (b1) && std::isfinite (b2)
            && std::isfinite (a1) && std::isfinite (a2);
    }
};

} // namespace bmo::deq

#pragma once

#include <algorithm>
#include <cmath>

namespace bmo::sat
{

//==============================================================================
/** log(cosh(u)), accurate at both ends.

    The obvious stable-for-large-u form, |u| + log1p(exp(-2|u|)) - log 2, is
    catastrophic for small u: the last two terms cancel to the true value of
    u^2/2, which for u near zero is far below the rounding error of either.
    That matters more than it looks, because ADAA divides the difference of two
    antiderivatives by a small dx and so amplifies any error in them by 1/dx.

    Writing the correction as log1p(expm1(-2a)/2) keeps both limits exact:
    expm1 and log1p are precisely the routines that do not lose the small
    difference, and at a = 0 the whole term is exactly zero.
*/
inline double logCosh (double u) noexcept
{
    const auto a = std::abs (u);
    return a + std::log1p (std::expm1 (-2.0 * a) * 0.5);
}

//==============================================================================
/** The waveshaper, and the whole character of the plugin:

        shape(x) = (tanh(a * x + b) - tanh(b)) / a

    a is the drive. b is a fixed offset, and it is the reason this sounds the
    way it does rather than like every other soft clipper.

    A symmetric curve is an odd function, and an odd function produces only odd
    harmonics. Third and fifth are what read as grit, edge and -- at the top of
    the spectrum, where plenty of them land -- sizzle. What reads as warmth is
    even: the second harmonic is an octave, so it stays consonant with whatever
    produced it, and the fourth is two octaves. Producing any even content at
    all requires the curve to treat the two polarities differently. Offsetting
    the operating point does exactly that, which is what a single-ended valve
    or class-A stage does by construction and what a push-pull one is built
    specifically to avoid.

    Two shapes were fitted to the reference here, and the choice between them
    is worth recording because both hit the target numbers. A curve made
    asymmetric by driving its two halves at different rates matches the
    reference's average gains just as well, and measures its second harmonic
    about 4 dB *below* its third -- the balance of a mostly-odd distortion,
    which is the anti-reference's character rather than the reference's. The
    offset form, fitted to the same two numbers, puts the second harmonic
    around 6 dB above the third. Same asymmetry on paper, opposite sound. The
    average-gain measurement alone does not tell them apart, which is worth
    knowing before trusting it on its own.

    kBias is frozen and fitted, not chosen: over a voice at a nominal working
    level, at the drive the panel's default produces, this curve's average gain
    is 0.620 across the positive excursions and 0.844 across the negative ones
    -- against the reference's 0.62 and 0.84, an asymmetry of 0.224 against
    0.22. See `measure fit` and docs/plan.md.

    The offset is held in the driven domain, so scaling a scales the whole
    curve without reshaping it: shape(x) is exactly g(a*x)/a for one fixed g.
    The ratio between the halves, and with it the balance of even to odd
    content, is therefore identical at every drive setting, and only the
    intensity moves. That is a requirement of the brief, and this is the line
    that satisfies it. The offset is not on the panel either: a control that
    could take the asymmetry out would let the plugin sound like something
    else, which is the opposite of what a character box is for.

    ADAA: evaluating a nonlinearity pointwise generates harmonics above Nyquist
    that fold back as inharmonic rubbish. Instead integrate it and take the
    difference quotient,

        y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])

    which is the average of the shaping function over the segment the signal
    actually traversed. tanh has a closed-form antiderivative in log(cosh(.)),
    so this costs little more than the shaper itself.

    What this class hands back is the residual, shape(x) - x, rather than
    shape(x). The fundamental is never attenuated on its way through here;
    where the distortion is added back, and in what proportion at which
    frequencies, is DspCore's business and is the other half of the design.
*/
class AsymmetricShaper
{
public:
    /** The offset, in the driven domain. */
    static constexpr double kBias = 0.263;

    void setDrive (float newDrive) noexcept
    {
        const auto updated = std::max ((double) newDrive, 1.0e-3);

        // Bit-exact comparison, deliberately: the smoother snaps to its target
        // once inside epsilon, so a settled control compares identical and the
        // state rebuild below is skipped entirely. Written with < rather than
        // == to say that this is intended rather than an oversight.
        if (! (updated < drive) && ! (drive < updated))
            return;

        drive = updated;

        // The running state has to be rebuilt against the new curve, and this
        // is not a nicety.
        //
        // ADAA carries F(x[n-1]) from one sample to the next and divides the
        // difference of two antiderivatives by dx. Change the drive without
        // this line and the numerator becomes the difference between two
        // *different* functions -- an arbitrarily large number divided by a
        // possibly tiny dx. Measured: sweeping Drive by hand produced single
        // samples over thirty times full scale, five thousand times the
        // largest step the programme itself was making. That is the loud
        // scratching noise anyone gets dragging the control during playback,
        // and it is why this class must never be given a new drive without
        // being told where it currently is.
        previousR = residualAntiderivative (previousX);
    }

    void reset() noexcept
    {
        previousX = 0.0;
        previousR = residualAntiderivative (0.0);
    }

    /** The anti-aliased distortion the curve adds, on its own: shape(x) - x,
        with no fundamental in it. */
    float processResidual (float x) noexcept
    {
        const double xd = x;
        const auto r  = residualAntiderivative (xd);
        const auto dx = xd - previousX;

        // The difference quotient is ill-conditioned when the signal barely
        // moves; fall back to the residual at the midpoint.
        const auto correction = std::abs (dx) > 1.0e-9 ? (r - previousR) / dx
                                                       : residual (0.5 * (xd + previousX));

        previousX = xd;
        previousR = r;
        return (float) correction;
    }

    /** The curve itself, with no anti-aliasing. The tests and the measurement
        harness use it to look at the shape directly. */
    double shape (double x) const noexcept
    {
        return (std::tanh (drive * x + kBias) - tanhBias()) / drive;
    }

    double residual (double x) const noexcept
    {
        return shape (x) - x;
    }

private:
    /** Only the nonlinear part is anti-aliased.

        Applied to the whole shaper, the ADAA difference quotient costs real
        high-frequency response: where the curve is locally linear the quotient
        reduces exactly to (x[n] + x[n-1]) / 2, a two-point moving average of
        magnitude cos(pi*f/fs). That is -3 dB at 12 kHz for a 48 kHz chain --
        a tone control nobody asked for, and on this plugin it would land
        squarely on the band the whole design exists to fill.

        Splitting shape(x) = x + residual(x) and anti-aliasing only the
        residual leaves the linear path untouched, so a signal below the knee
        passes with its treble intact. The residual is the distortion itself,
        so the slight smoothing it still receives does no harm.
    */
    double residualAntiderivative (double x) const noexcept
    {
        return antiderivative (x) - 0.5 * x * x;
    }

    double antiderivative (double x) const noexcept
    {
        const auto u = drive * x + kBias;
        return (logCosh (u) - tanhBias() * u) / (drive * drive);
    }

    static double tanhBias() noexcept
    {
        static const double value = std::tanh (kBias);
        return value;
    }

    double drive = 1.0;
    double previousX = 0.0, previousR = 0.0;
};

} // namespace bmo::sat

#pragma once

#include <algorithm>
#include <cmath>

namespace bmo::opto
{

/** The gain cell: a feedback envelope follower feeding a soft-knee gain
    computer, standing in for the T4 opto-electrical cell neither Teletronix
    nor Empirical Labs publish a circuit for.

    Two things below are the model, not an implementation detail, and both
    come from how a real opto cell behaves rather than from a textbook
    compressor:

    Feedback, not feedforward. `process()` returns `input * gainLin` using
    the gain the *previous* sample's envelope produced, then updates the
    envelope from that already-reduced result -- one sample of delay, which
    is how every analogue feedback compressor works: the detector watches the
    output of the cell, not the signal arriving at it. This is what makes the
    unit self-limiting and program-dependent rather than reactive; a
    feedforward detector (read the input, then decide the gain) is simpler
    but is not what an LA-2A does.

    Program-dependent release. The EL panel lights quickly and dims slowly,
    and the harder *and longer* it has been driven the slower it dims -- the
    "two-stage" release everyone describes. The "longer" part needs its own
    state: reduction alone is instantaneous and forgets the moment the level
    drops, so a brief transient and a two-second-long hit that reach the same
    peak would release at the same speed, which is not what the real cell
    does. `chargeDb` is that memory -- a second, slower follower of
    `reductionDb` that takes `kChargeAttackTauSec` to climb toward a
    sustained reduction and `kReleaseSlowTauSec` to forget it again, standing
    in for the EL panel's phosphor building up a charge under sustained
    drive. The envelope's own release time constant is read from *this*, not
    from the instantaneous reduction, which is what makes a long hit actually
    take longer to let go of than a short one at the same level. Attack is
    fixed and fast, because the real thing has no attack control either.

    `curveFor()` is the other half of the character, and it is where CRUSH's
    two source hardware units diverge and get combined: the threshold and
    ratio move together as CRUSH rises, the way the LA-2A's single Peak
    Reduction knob does -- there is no separate threshold or ratio control on
    the real thing, so there isn't one here. The knee narrows as CRUSH rises,
    which is not an LA-2A trait; it is borrowed from the Distressor's 10:1
    "Opto" ratio, which several engineers who have used both describe as a
    harder, more sudden catch than a real opto compressor's. So: LA-2A for
    the topology and the release, Distressor-opto for how hard the knee
    grabs. The constants are a starting point tuned by ear, not measured from
    either unit -- see products/AGENTS.md for the research this is drawn
    from, and retune them once this is actually loaded in a DAW.
*/
class OptoDetector
{
public:
    struct Curve
    {
        float thresholdDb;
        float ratio;
        float kneeDb;
    };

    /** CRUSH, 0-100 on the panel, mapped to a threshold/ratio/knee triple.
        Not linear in "amount of compression" by design: turning CRUSH up
        does three things the real Peak Reduction knob does at once. */
    static Curve curveFor (float crushPercent) noexcept
    {
        const auto c = std::clamp (crushPercent, 0.0f, 100.0f) / 100.0f;

        return {
            -8.0f  + c * -30.0f,   // threshold: -8 dB at Crush 0, -38 dB at Crush 100
             2.0f  + c * 7.0f,     // ratio: 2:1 softly rising to 9:1
            16.0f  - c * 10.0f,    // knee: 16 dB soft at low Crush, 6 dB harder at high Crush
        };
    }

    void prepare (double sampleRate) noexcept
    {
        rate = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        envelopeLin  = 0.0f;
        gainLin      = 1.0f;
        reductionDb  = 0.0f;
        chargeDb     = 0.0f;
    }

    /** One sample through the cell. Returns the already-reduced sample,
        before makeup gain -- the caller applies LEVEL on top of this. */
    float process (float inputSample, const Curve& curve) noexcept
    {
        const auto y = inputSample * gainLin;

        const auto levelLin = std::abs (y);
        const auto rising   = levelLin > envelopeLin;

        const auto attackCoeff = coeffFor (kAttackTauSec);

        // Release reads the *charge*, not the instantaneous reduction -- see
        // the class comment for why that is the difference that gives a
        // sustained hit a longer tail than a transient at the same level.
        const auto depth       = std::clamp (chargeDb / 20.0f, 0.0f, 1.0f);
        const auto releaseTau  = kReleaseFastTauSec + (kReleaseSlowTauSec - kReleaseFastTauSec) * depth;
        const auto releaseCoeff = coeffFor (releaseTau);

        envelopeLin += (rising ? attackCoeff : releaseCoeff) * (levelLin - envelopeLin);

        const auto envelopeDb = 20.0f * std::log10 (std::max (envelopeLin, 1.0e-6f));
        reductionDb = std::clamp (kneeReductionDb (envelopeDb, curve), 0.0f, 40.0f);
        gainLin     = std::pow (10.0f, -reductionDb / 20.0f);

        // The charge climbs towards a sustained reduction over
        // kChargeAttackTauSec and only forgets it over kReleaseSlowTauSec, so
        // a hit shorter than the attack time barely charges it at all.
        const auto chargeCoeff = reductionDb > chargeDb ? coeffFor (kChargeAttackTauSec)
                                                         : coeffFor (kReleaseSlowTauSec);
        chargeDb += chargeCoeff * (reductionDb - chargeDb);

        return y;
    }

    /** Gain reduction the cell is applying right now, in dB, always >= 0. */
    float currentReductionDb() const noexcept { return reductionDb; }

private:
    float coeffFor (float tauSec) const noexcept
    {
        return 1.0f - std::exp (-1.0f / (float) (std::max (rate, 1.0) * (double) tauSec));
    }

    /** The soft-knee gain computer from Reiss & McPherson's compressor
        tutorial, restated to return the reduction directly rather than the
        output level: for a ratio R and knee width W either side of threshold
        T, the input-output gain difference is a smooth parabola inside the
        knee and a straight line beyond it. Continuous and monotonic in
        `levelDb` by construction, which is what the DSP tests hold it to --
        the exact threshold/ratio/knee numbers are free to move. */
    static float kneeReductionDb (float levelDb, const Curve& curve) noexcept
    {
        const auto diff = levelDb - curve.thresholdDb;
        const auto half = curve.kneeDb * 0.5f;

        if (diff <= -half)
            return 0.0f;

        if (diff < half)
        {
            const auto t = diff + half;
            return (1.0f - 1.0f / curve.ratio) * (t * t) / (2.0f * curve.kneeDb);
        }

        return (1.0f - 1.0f / curve.ratio) * diff;
    }

    static constexpr float kAttackTauSec        = 0.008f;   // ~8 ms: fixed, fast, no control for it
    static constexpr float kReleaseFastTauSec   = 0.06f;    // a light touch lets go quickly
    static constexpr float kReleaseSlowTauSec   = 1.4f;     // a heavy, sustained one takes its time
    static constexpr float kChargeAttackTauSec  = 0.3f;     // how long a hit has to last to "count" as sustained

    double rate = 44100.0;
    float  envelopeLin = 0.0f;
    float  gainLin     = 1.0f;
    float  reductionDb = 0.0f;
    float  chargeDb    = 0.0f;
};

} // namespace bmo::opto

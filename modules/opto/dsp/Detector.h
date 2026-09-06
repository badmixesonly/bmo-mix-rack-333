#pragma once

#include <algorithm>
#include <cmath>

namespace bmo::opto
{

/** Threshold/ratio/knee for one sample. Ratio and knee are fixed per mode --
    see curveForLa2a() / curveForDistressor() -- only threshold moves with
    CRUSH, because neither real unit has a ratio control; CRUSH is a stand-in
    for the Peak Reduction knob, which pushes more signal over a fixed
    circuit's threshold rather than reshaping the circuit itself. */
struct Curve
{
    float thresholdDb;
    float ratio;
    float kneeDb;
};

/** The soft-knee gain computer from Reiss & McPherson's compressor tutorial,
    restated to return the reduction directly: for a ratio R and knee width W
    either side of threshold T, the input-output gain difference is a smooth
    parabola inside the knee and a straight line beyond it. Shared by both
    modes -- it's generic knee math, not part of what makes either unit
    sound like itself. */
inline float kneeReductionDb (float levelDb, const Curve& curve) noexcept
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

inline float coeffFor (float tauSec, double rate) noexcept
{
    return 1.0f - std::exp (-1.0f / (float) (std::max (rate, 1.0) * (double) tauSec));
}

/** CRUSH, 0-100 on the panel, mapped to threshold only. LA-2A: ~3:1, a soft
    16 dB knee -- both fixed, per the digest's "effectively fixed/soft-knee,
    not a user ratio control." */
inline Curve curveForLa2a (float crushPercent) noexcept
{
    const auto c = std::clamp (crushPercent, 0.0f, 100.0f) / 100.0f;
    return { -8.0f + c * -30.0f, 3.0f, 16.0f };
}

/** Distressor's 10:1 "Opto" ratio setting: fixed 10:1, a harder/shorter 6 dB
    knee -- "reminiscent of 60s/70s gear," a harder catch than a true optical
    unit, per the digest. Same threshold sweep as LA-2A so CRUSH means the
    same thing (how much signal crosses the fixed circuit's threshold) in
    both modes; the character difference is ratio, knee, topology and
    release, not the sweep itself. */
inline Curve curveForDistressor (float crushPercent) noexcept
{
    const auto c = std::clamp (crushPercent, 0.0f, 100.0f) / 100.0f;
    return { -8.0f + c * -30.0f, 10.0f, 6.0f };
}

//==============================================================================
/** LA-2A: a feedback cell with a genuine dosage-dependent release.

    Feedback, not feedforward. `process()` returns `input * gainLin` using
    the gain the *previous* sample's envelope produced, then updates the
    envelope from that already-reduced result -- the detector watches the
    output of the cell, not the signal arriving at it, which is what a real
    photo-electric feedback loop does and what makes it self-limiting.

    Two-stage AND dosage-dependent release. The 60 ms-to-50%-recovery stage
    is instantaneous and reductionDb-driven, same as ever. What is new here:
    the slow stage's own time constant is not one fixed number, and not just
    a one-pole low-pass of reductionDb either -- a low-pass alone saturates
    within about a second regardless of how much longer the hit continues,
    so it can't tell a 2-second hit from a 10-second one, which the T4
    cell's CdS photoresistor demonstrably can (its recovery is governed by
    at least two charge-trap populations with different relaxation rates,
    which is exactly why it has "memory" of exposure in the first place).
    `dosageSec` is a second, slower accumulator of *how long* the cell has
    been meaningfully loaded, not just *whether* -- it keeps climbing for as
    long as reductionDb stays engaged, and the slow release tau is a
    continuous, saturating function of it (kReleaseSlowMinTauSec at zero
    dosage, sliding up toward kReleaseSlowMaxTauSec only after real sustained
    exposure). A short transient barely moves it; a long, loud hit does.

    Attack is fixed at ~10 ms, per every source consulted -- there's no
    evidence (here or in the digest) that the real cell's attack is
    level-dependent the way its release is, so this doesn't invent one. */
class La2aCell
{
public:
    void prepare (double sampleRate) noexcept { rate = sampleRate; reset(); }

    void reset() noexcept
    {
        envelopeLin = 0.0f;
        gainLin     = 1.0f;
        reductionDb = 0.0f;
        chargeDb    = 0.0f;
        dosageSec   = 0.0f;
    }

    /** One sample through the cell. Returns the already-reduced sample,
        before makeup gain. */
    float process (float inputSample, const Curve& curve) noexcept
    {
        const auto y = inputSample * gainLin;
        updateFromOutputSample (y, curve);
        return y;
    }

    /** For stereo link: advance the cell's state from an output sample that
        was computed elsewhere (e.g. the louder of two linked channels,
        already scaled by this cell's own currentGainLin()), without also
        re-deriving that sample here. */
    void updateFromOutputSample (float y, const Curve& curve) noexcept
    {
        const auto levelLin = std::abs (y);
        const auto rising   = levelLin > envelopeLin;

        const auto attackCoeff = coeffFor (kAttackTauSec, rate);

        const auto dosageT      = std::clamp (dosageSec / kDosageGrowthSec, 0.0f, 4.0f);
        const auto dosageAmount = 1.0f - std::exp (-dosageT);
        const auto slowTau      = kReleaseSlowMinTauSec
                                     + (kReleaseSlowMaxTauSec - kReleaseSlowMinTauSec) * dosageAmount;

        const auto depth        = std::clamp (chargeDb / 20.0f, 0.0f, 1.0f);
        const auto releaseTau   = kReleaseFastTauSec + (slowTau - kReleaseFastTauSec) * depth;
        const auto releaseCoeff = coeffFor (releaseTau, rate);

        envelopeLin += (rising ? attackCoeff : releaseCoeff) * (levelLin - envelopeLin);

        const auto envelopeDb = 20.0f * std::log10 (std::max (envelopeLin, 1.0e-6f));
        reductionDb = std::clamp (kneeReductionDb (envelopeDb, curve), 0.0f, 40.0f);
        gainLin     = std::pow (10.0f, -reductionDb / 20.0f);

        const auto chargeCoeff = reductionDb > chargeDb ? coeffFor (kChargeAttackTauSec, rate)
                                                         : coeffFor (kReleaseSlowMinTauSec, rate);
        chargeDb += chargeCoeff * (reductionDb - chargeDb);

        // Dosage climbs for as long as the cell is meaningfully engaged and
        // forgets slowly once it isn't -- this is what lets a long hit reach
        // a slower release than a short one at the same peak, continuously
        // rather than as a single fast/slow pick.
        const auto dt = (float) (1.0 / std::max (rate, 1.0));
        const auto engaged = reductionDb > kDosageEngageDb;
        dosageSec += engaged ? dt : -dt * (kDosageGrowthSec / kDosageForgetSec);
        dosageSec = std::clamp (dosageSec, 0.0f, kDosageGrowthSec * 4.0f);
    }

    /** Gain reduction the cell is applying right now, in dB, always >= 0. */
    float currentReductionDb() const noexcept { return reductionDb; }

    /** The gain the cell is applying right now -- read before the next
        sample so a linked channel can apply the same gain this cell decided
        without going through process() twice. */
    float currentGainLin() const noexcept { return gainLin; }

private:
    static constexpr float kAttackTauSec         = 0.010f;  // ~10 ms, fixed -- no source supports it moving
    static constexpr float kReleaseFastTauSec    = 0.06f;   // ~60 ms to the first 50% of recovery
    static constexpr float kReleaseSlowMinTauSec = 1.0f;    // slow tail floor: a hit just past "sustained"
    static constexpr float kReleaseSlowMaxTauSec = 15.0f;   // slow tail ceiling: a long, heavy hit
    static constexpr float kChargeAttackTauSec   = 0.3f;    // how long a hit has to last to "count"
    static constexpr float kDosageEngageDb       = 1.0f;    // reduction below this doesn't accrue dosage
    static constexpr float kDosageGrowthSec      = 3.0f;    // how long sustained drive takes to matter
    static constexpr float kDosageForgetSec      = 4.0f;    // how long the cell takes to forget exposure

    double rate = 44100.0;
    float envelopeLin = 0.0f;
    float gainLin     = 1.0f;
    float reductionDb = 0.0f;
    float chargeDb    = 0.0f;
    float dosageSec   = 0.0f;
};

//==============================================================================
/** Distressor, 10:1 "Opto" ratio: a feedforward cell with electronically-
    timed auto-release -- a different topology from the LA-2A on purpose.
    The Distressor is a VCA-based feedforward compressor; its Opto setting
    switches in dedicated detector/timing circuitry built to *emulate* an
    optical unit's feel, not an actual photoresistor. There's no published
    schematic and no source found describing multi-population charge-trap
    behavior for it the way real CdS cells have -- so unlike La2aCell, this
    doesn't get a continuously-growing dosage state. A single charge-driven
    blend between a fast floor and this mode's own (much longer, ~20 s)
    ceiling is the more honest model: it's the standard way this class of
    analogue auto-release circuit is built (a capacitor charged by gain-
    reduction depth/duration sets the release rate), and there's no evidence
    to justify claiming more precision than that.

    Feedforward: the detector reads the input directly, not the cell's own
    output, matching the real unit's topology. Attack is static at ~10 ms
    and does not lengthen with programme material, per the digest -- unlike
    the LA-2A, this is stated explicitly rather than just unconfirmed. */
class DistressorCell
{
public:
    void prepare (double sampleRate) noexcept { rate = sampleRate; reset(); }

    void reset() noexcept
    {
        envelopeLin = 0.0f;
        gainLin     = 1.0f;
        reductionDb = 0.0f;
        chargeDb    = 0.0f;
    }

    /** One sample through the cell: feedforward, so the detector reads
        `inputSample` directly rather than the cell's own output. */
    float process (float inputSample, const Curve& curve) noexcept
    {
        updateFromInputSample (inputSample, curve);
        return inputSample * gainLin;
    }

    /** For stereo link: advance the cell's state from a detection sample
        computed elsewhere (e.g. the louder of two linked channels). */
    void updateFromInputSample (float x, const Curve& curve) noexcept
    {
        const auto levelLin = std::abs (x);
        const auto rising   = levelLin > envelopeLin;

        const auto attackCoeff = coeffFor (kAttackTauSec, rate);

        const auto depth        = std::clamp (chargeDb / 20.0f, 0.0f, 1.0f);
        const auto releaseTau   = kReleaseFastTauSec + (kReleaseSlowTauSec - kReleaseFastTauSec) * depth;
        const auto releaseCoeff = coeffFor (releaseTau, rate);

        envelopeLin += (rising ? attackCoeff : releaseCoeff) * (levelLin - envelopeLin);

        const auto envelopeDb = 20.0f * std::log10 (std::max (envelopeLin, 1.0e-6f));
        reductionDb = std::clamp (kneeReductionDb (envelopeDb, curve), 0.0f, 40.0f);
        gainLin     = std::pow (10.0f, -reductionDb / 20.0f);

        const auto chargeCoeff = reductionDb > chargeDb ? coeffFor (kChargeAttackTauSec, rate)
                                                         : coeffFor (kReleaseSlowTauSec, rate);
        chargeDb += chargeCoeff * (reductionDb - chargeDb);
    }

    float currentReductionDb() const noexcept { return reductionDb; }
    float currentGainLin() const noexcept { return gainLin; }

private:
    static constexpr float kAttackTauSec       = 0.010f; // ~10 ms, static -- confirmed non-adaptive
    static constexpr float kReleaseFastTauSec  = 0.06f;
    static constexpr float kReleaseSlowTauSec  = 20.0f;  // this mode's own ceiling, vs LA-2A's 15 s
    static constexpr float kChargeAttackTauSec = 0.3f;

    double rate = 44100.0;
    float envelopeLin = 0.0f;
    float gainLin     = 1.0f;
    float reductionDb = 0.0f;
    float chargeDb    = 0.0f;
};

//==============================================================================
/** A one-pole DC blocker: needed after La2aDrive's asymmetric term, which
    would otherwise push a DC offset through the rest of the chain. */
class DcBlocker
{
public:
    void reset() noexcept { x1 = y1 = 0.0f; }

    float process (float x) noexcept
    {
        constexpr float r = 0.995f;
        const auto y = x - x1 + r * y1;
        x1 = x; y1 = y;
        return y;
    }

private:
    float x1 = 0.0f, y1 = 0.0f;
};

/** LA-2A Drive: the 12AX7/12BH7 makeup stage, 6AQ5 and output transformer's
    mild, mostly low-order warmth -- modelled as a symmetric soft clip (odd
    harmonics, the bulk of any tube stage's output) plus a small asymmetric
    (quadratic) term that adds the low-order *even* harmonics a single-ended
    tube stage is known for, DC-blocked afterward since the asymmetry alone
    would offset the signal. Not level/GR-dependent -- Drive is on or off,
    not a knob, so this is one fixed, tasteful amount.

    `tanh (k * x) / k`, not `tanh (k * x) / tanh (k)`: the latter normalizes
    full-scale input to exactly unity, which sounds reasonable but means the
    *small*-signal gain is `k / tanh (k)` -- always greater than one, so a
    quiet passage comes out louder than it went in before any "warmth" is
    even audible. `/ k` instead gives unity gain at the origin (a quiet
    signal passes essentially untouched) and only compresses as level
    approaches and exceeds where the curve bends -- the shape a passive
    tube/transformer stage actually has, and the one testQuietSignalIsLeftAlone
    and testMakeupGainIsExact both hold this to. */
class La2aDrive
{
public:
    void reset() noexcept { dc.reset(); }

    float process (float x) noexcept
    {
        constexpr float k = 0.6f;
        constexpr float evenAmount = 0.18f;

        const auto shaped = std::tanh (k * x) / k;
        const auto biased  = shaped + evenAmount * (shaped * shaped) * (x < 0.0f ? -1.0f : 1.0f);

        return dc.process (biased);
    }

private:
    DcBlocker dc;
};

/** Distressor Drive: the tape-like 3rd-harmonic flattening stage (the
    grittier of its two switchable harmonic options, chosen over the gentler
    Class-A 2nd-harmonic stage as the character this toggle represents) --
    a purely symmetric soft clip, which is what generates odd harmonics
    (3rd, 5th, ...) without needing a separate DC blocker. A larger `k` than
    La2aDrive's on purpose -- see La2aDrive for why `/ k` and not `/ tanh (k)`
    -- so it still passes a quiet signal through near enough unchanged but
    compresses considerably more at the levels it's meant to be heard on,
    reading as grittier and further from the LA-2A's own tone. */
class DistressorDrive
{
public:
    void reset() noexcept {}

    float process (float x) noexcept
    {
        constexpr float k = 1.2f;
        return std::tanh (k * x) / k;
    }
};

} // namespace bmo::opto

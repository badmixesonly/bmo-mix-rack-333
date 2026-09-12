#pragma once

#include "modules/tune/dsp/Detector.h"
#include "modules/tune/dsp/Scale.h"
#include <array>

namespace bmo::tune
{

/** Everything the correction law reads, in real units. */
struct CorrectionSettings
{
    double refA = 440.0;                  ///< concert A, 380-480
    int key = 0;                          ///< 0 = C
    ScaleType scale = ScaleType::chromatic;
    NoteMask allowed = kAllNotes;         ///< per-note allow map, ANDed with the scale

    double retuneMs = 0.0;                ///< one-pole time constant; 0 is a true snap
    double vibratoAmount = 0.0;           ///< beta: 0 flattens vibrato, 1 keeps it, >1 exaggerates
    double flex = 0.0;                    ///< 0..1 soft-knee deadzone; 0 is off
    double hysteresisCents = 8.0;         ///< see nearestAllowed()

    /** At vibrato 0 the note follows the raw pitch, so a singer sitting near
        a boundary between two scale notes flipped between them with every
        wobble -- "hunting", in Frosty's blind test (2026-09-11), who chose
        "hold the note steadier". A switch now needs the new note to be
        noteClearCents closer than the held one (the pitch 30 cents across the
        midpoint: a real step arrives 100-200 cents closer, at once), or to
        stay closer, past the hysteresis, for noteDwellMs. 30 was tried first
        and was too little: on Failure the singer sits on D#, midway between
        D and E, and a +/-15 cent wobble there still flipped every time. */
    double noteClearCents = 60.0;
    double noteDwellMs = 40.0;

    /** What a hold may cost before it is cut, in cents x milliseconds, and the
        pull it carries for free.

        Round three (2026-09-11) heard the flat dwell as pops at retune 20 ms,
        and on Failure every splice it added fell while it held the target off
        the note the singer had reached, pulling about 91 cents. A held note is
        shifted by that pull for as long as it is held, and the engine's read
        drifts at the same rate; far enough, and it splices a whole period
        (see ClassicEngine). So a hold is billed by the pull it cannot afford
        -- whatever exceeds noteHoldFreeCents -- times how long it carries it.

        The allowance is what keeps the thing the dwell was built for. A
        vibrato that only just crosses a boundary sits no more than 60 cents
        from the held note and drifts too slowly to splice, so it is never
        billed and keeps its whole dwell. A singer who has properly moved is
        pulled harder, runs the bill up in a few milliseconds, and switches. */
    double noteHoldFreeCents = 60.0;
    double noteHoldBudgetCentMs = 320.0;

    double clarityLo = 0.60, clarityHi = 0.85;   ///< confidence ramp (spec §4.4)
    double maxCorrectionCents = 1200.0;          ///< hard clamp (spec §6.1)
};

/** What the law decided on the latest sample; --dump-analysis writes it. */
struct CorrectionState
{
    double pitchIn = 0.0;      ///< semitones, as detected
    double target = 0.0;       ///< semitones: the quantized note
    int note = -1;             ///< the quantized note, -1 for none
    double errorCents = 0.0;   ///< target - input
    double appliedCents = 0.0; ///< a_final: what the engine is told to do
    int noteChanges = 0;       ///< running count, for the flip-flop metric
};

/** Detection in, cents of correction out, one sample at a time (spec §4).

    The chain, in order:

        confirm    a pitch jump of more than 3/4 semitone waits for the next
                   estimate to agree; see confirmPitch() for why this replaces
                   the spec's median on the note
        quantize   nearest allowed note, with hysteresis toward the held note;
                   at vibrato 0, a switch by a small margin must also hold
                   for noteDwellMs (see holdOrSwitch())
        error      e = 100 (target - p_in)
        vibrato    e - beta (e - LP3Hz(e)): correct the slow part of the
                   error, pass beta of the fast part
        flex       e g(|e|), a C1 soft knee; identity when flex is 0
        retune     one-pole toward that, alpha = exp(-1 / (fs tau)); tau = 0
                   bypasses the pole entirely, so the snap is exact
        confidence scaled by clarity between the voicing thresholds
        voicing    ramped in over a period after lock, out over 10 ms

    The vibrato split is written on the error rather than on the pitch as
    the spec writes it. The two are the same thing -- with the target held,
    e - LP(e) is exactly -(p_in - LP(p_in)) -- but on the error a note change
    is a clean step in the target that the slow state can be shifted by, so
    the split survives a legato note change without re-injecting the whole
    interval as "vibrato".
*/
class CorrectionLaw
{
public:
    void prepare (double sampleRate);
    void reset();
    void setSettings (const CorrectionSettings&) noexcept;

    /** One sample. `evaluated` is the detector's evaluatedThisSample(). */
    double tick (const PitchEstimate&, bool evaluated) noexcept;

    const CorrectionState& state() const noexcept { return st; }

    /** The flex soft knee on its own, for its test: gain in [0, 1]. */
    static double flexGain (double absCents, double flex) noexcept;

private:
    bool decideNote (double pitchForDecision, int& note) noexcept;
    int holdOrSwitch (double pitch, int candidate) noexcept;
    double confirmPitch (double latest) noexcept;
    void setNote (int note) noexcept;

    CorrectionSettings s;
    double fs = 48000.0;

    double retuneAlpha = 0.0, slowCoeff = 0.0, confidenceCoeff = 0.0;
    double decideCoeff = 0.0;
    int fadeOutSamples = 480;

    // Detection-side state.
    double pitchIn = 0.0, pitchSlow = 0.0;
    bool havePitch = false, voiced = false;
    double clarity = 0.0;
    double period = 0.0;

    // Target-side state.
    int note = -1;
    bool haveNote = false;
    double pendingJump = 0.0;
    bool havePendingJump = false;
    double target = 0.0;

    // A note switch waiting out noteDwellMs: which note, since which sample,
    // what the hold has cost so far (cents x ms) and when it was last billed.
    int pendingNote = -1;
    long long pendingNoteSince = 0, samples = 0;
    long long dwellSamples = 0, pendingBilledAt = 0;
    double pendingCostCentMs = 0.0;

    // The law's own state.
    double errorSlow = 0.0, applied = 0.0, confidence = 0.0, gate = 0.0;

    CorrectionState st;
};

} // namespace bmo::tune

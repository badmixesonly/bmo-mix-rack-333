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
    double glideMs = 0.0;                 ///< target slew on note change
    bool glideAllowed = false;            ///< CLASSIC never glides (spec §4.5)
    double hysteresisCents = 8.0;         ///< see nearestAllowed()

    double clarityLo = 0.60, clarityHi = 0.85;   ///< confidence ramp (spec §4.4)
    double maxCorrectionCents = 1200.0;          ///< hard clamp (spec §6.1)
};

/** What the law decided on the latest sample; --dump-analysis writes it. */
struct CorrectionState
{
    double pitchIn = 0.0;      ///< semitones, as detected
    double target = 0.0;       ///< semitones, after glide
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
        quantize   nearest allowed note, with hysteresis toward the held note
        glide      target slew on a note change -- HYBRID only
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

    /** The retune knob (0-100) as a time constant: exponential, 0 ms at 0
        and 400 ms at 100 (spec §4.2). Zero must be reachable exactly. */
    static double retuneMsFromKnob (double knob) noexcept;

    /** The flex soft knee on its own, for its test: gain in [0, 1]. */
    static double flexGain (double absCents, double flex) noexcept;

private:
    bool decideNote (double pitchForDecision, int& note) noexcept;
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
    double target = 0.0, glideFrom = 0.0, glideTo = 0.0;
    int glideLength = 0, glidePosition = 0;

    // The law's own state.
    double errorSlow = 0.0, applied = 0.0, confidence = 0.0, gate = 0.0;

    CorrectionState st;
};

} // namespace bmo::tune

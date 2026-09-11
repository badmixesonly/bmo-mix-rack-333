#include "modules/tune/dsp/CorrectionLaw.h"
#include "modules/tune/dsp/Pitch.h"
#include <algorithm>
#include <cmath>

namespace bmo::tune
{

namespace
{
    double onePole (double sampleRate, double seconds)
    {
        return seconds > 0.0 ? 1.0 - std::exp (-1.0 / (sampleRate * seconds)) : 1.0;
    }

    int medianOf3 (int a, int b, int c)
    {
        return std::max (std::min (a, b), std::min (std::max (a, b), c));
    }
}

double CorrectionLaw::retuneMsFromKnob (double knob) noexcept
{
    // 400 (2^(8k) - 1) / (2^8 - 1), k in [0, 1]: 0 at 0, 3 ms at 20, 24 ms at
    // 50, 400 ms at 100. The low end is where hard tuning lives, so it gets
    // most of the knob's travel; a linear 0-400 ms knob would spend its
    // first 5 % on everything anyone sets for the effect.
    const auto k = std::clamp (knob, 0.0, 100.0) / 100.0;
    return k <= 0.0 ? 0.0 : 400.0 * (std::exp2 (8.0 * k) - 1.0) / 255.0;
}

double CorrectionLaw::flexGain (double u, double flex) noexcept
{
    // The spec's knee (§4.3a) with its own curve corrected: it writes
    // ((u - u0) / (u1 - u0))^2 and calls it "smoothstep, C1 at both ends",
    // but a square has slope 2 / (u1 - u0) at u1, so the applied correction
    // e g(|e|) kinks where the knee meets full correction. The real
    // smoothstep, 3t^2 - 2t^3, is flat at both ends and is what is used.
    const auto f = std::clamp (flex, 0.0, 1.0);
    const auto u0 = 20.0 * f;   // leave alone below this many cents
    const auto u1 = 50.0 * f;   // correct fully above this many

    if (u1 <= 0.0 || u >= u1)
        return 1.0;
    if (u <= u0)
        return 0.0;

    const auto t = (u - u0) / (u1 - u0);
    return t * t * (3.0 - 2.0 * t);
}

void CorrectionLaw::prepare (double sampleRate)
{
    fs = sampleRate;
    slowCoeff = onePole (fs, 1.0 / (2.0 * kPi * 3.0));        // 3 Hz corner (spec §4.3b)
    decideCoeff = slowCoeff;
    confidenceCoeff = onePole (fs, 0.005);
    fadeOutSamples = std::max (1, (int) std::lround (0.010 * fs));
    setSettings (s);
    reset();
}

void CorrectionLaw::reset()
{
    pitchIn = pitchSlow = 0.0;
    havePitch = voiced = false;
    clarity = period = 0.0;
    note = -1;
    haveNote = false;
    recentCount = 0;
    target = glideFrom = glideTo = 0.0;
    glideLength = glidePosition = 0;
    errorSlow = applied = confidence = gate = 0.0;
    st = {};
}

void CorrectionLaw::setSettings (const CorrectionSettings& settings) noexcept
{
    s = settings;
    s.refA = std::clamp (s.refA, 380.0, 480.0);
    s.vibratoAmount = std::clamp (s.vibratoAmount, 0.0, 1.5);

    // Exactly zero, not "very fast": at tau = 0 the pole is bypassed.
    retuneAlpha = s.retuneMs > 0.0 ? std::exp (-1.0 / (fs * s.retuneMs * 0.001)) : 0.0;
}

bool CorrectionLaw::decideNote (double pitch, const MidiTarget& midi, int& decided) noexcept
{
    const auto latch = s.midiLatch;

    if (s.midiMode == MidiTarget::Mode::target)
    {
        if (midi.targetNote (latch, decided))
            return true;

        if (s.midiRequired)
            return false;
    }

    NoteMask mask = (NoteMask) (scaleMask (s.scale, s.key) & s.allowed);

    if (s.midiMode == MidiTarget::Mode::scale)
    {
        if (midi.anyHeld (latch))
            mask = midi.heldMask (latch);
        else if (s.midiRequired)
            return false;
    }

    int raw = 0;
    if (! nearestAllowed (pitch, mask, note, haveNote, s.hysteresisCents, raw))
        return false;

    // Median of the last three raw decisions (guard 5 in spec §3.5): on the
    // note decision, never on the period, so it costs no audio latency --
    // one evaluation of delay on a note change, a quarter period at most.
    if (recentCount == 0)
    {
        recent.fill (raw);
        recentCount = 3;
    }
    else
    {
        recent[0] = recent[1];
        recent[1] = recent[2];
        recent[2] = raw;
    }

    decided = medianOf3 (recent[0], recent[1], recent[2]);
    return true;
}

void CorrectionLaw::setNote (int newNote) noexcept
{
    if (haveNote && newNote == note)
        return;

    if (haveNote)
    {
        // Carry the vibrato split's slow state across the step, so the fast
        // part of the error stays what it was rather than becoming the whole
        // interval for the next few hundred milliseconds.
        errorSlow += 100.0 * (newNote - note);
        ++st.noteChanges;

        glideFrom = target;
        glideTo = newNote;
        glideLength = s.glideAllowed ? (int) std::lround (s.glideMs * 0.001 * fs) : 0;
        glidePosition = 0;

        if (glideLength <= 0)
            target = newNote;
    }
    else
    {
        // The first note of a phrase: nothing to glide from, and the split's
        // slow state starts at this note's own error rather than the last
        // phrase's.
        target = glideFrom = glideTo = newNote;
        glideLength = glidePosition = 0;
        errorSlow = 100.0 * (newNote - pitchIn);
    }

    note = newNote;
    haveNote = true;
}

double CorrectionLaw::tick (const PitchEstimate& e, bool evaluated, const MidiTarget& midi) noexcept
{
    voiced = e.voiced;

    if (evaluated)
    {
        clarity = e.clarity;

        if (e.voiced && e.period > 0.0)
        {
            period = e.period;
            pitchIn = pitch::semitonesFromHz (fs / e.period, s.refA);

            if (e.onset || ! havePitch)
            {
                // A new note starts from itself: no slow state carried from
                // the last phrase, no correction carried either.
                pitchSlow = pitchIn;
                recentCount = 0;
                haveNote = false;
                applied = 0.0;
            }

            havePitch = true;
        }
    }

    if (havePitch)
    {
        // The note decision follows the slow pitch when vibrato is being kept,
        // so a vibrato straddling a boundary does not flip the target twice a
        // cycle. A leap bigger than any vibrato snaps it, so a real interval
        // is not heard late.
        pitchSlow += decideCoeff * (pitchIn - pitchSlow);
        if (std::abs (pitchIn - pitchSlow) > 1.5)
            pitchSlow = pitchIn;

        if (evaluated && voiced)
        {
            int decided = 0;
            if (decideNote (s.vibratoAmount > 0.0 ? pitchSlow : pitchIn, midi, decided))
                setNote (decided);
            else
                haveNote = false;
        }
    }

    // Glide the target toward the note, linearly in semitones.
    if (haveNote && glideLength > 0 && glidePosition < glideLength)
    {
        ++glidePosition;
        target = glideFrom + (glideTo - glideFrom) * (double) glidePosition / (double) glideLength;
    }

    const auto active = havePitch && haveNote && voiced;
    double desired = 0.0;

    if (havePitch && haveNote)
    {
        const auto error = 100.0 * (target - pitchIn);
        errorSlow += slowCoeff * (error - errorSlow);

        const auto split = error - s.vibratoAmount * (error - errorSlow);
        desired = split * flexGain (std::abs (split), s.flex);
        st.errorCents = error;
    }

    // Retune. Frozen while unvoiced: the gate below fades the amount, and the
    // ratio holds where it was so a breath is not re-pitched on its way out.
    if (active)
        applied = retuneAlpha * applied + (1.0 - retuneAlpha) * desired;

    const auto conf = std::clamp ((clarity - s.clarityLo) / std::max (1.0e-6, s.clarityHi - s.clarityLo), 0.0, 1.0);
    confidence += confidenceCoeff * (conf - confidence);

    // In over one period after lock (the provisional-output policy of spec
    // §2.1: at an onset the plugin is a wire, and the correction arrives
    // within a cycle), out over 10 ms.
    const auto rampIn = std::max (1.0, period);
    gate = active ? std::min (1.0, gate + 1.0 / rampIn)
                  : std::max (0.0, gate - 1.0 / fadeOutSamples);

    const auto out = std::clamp (applied * confidence * gate,
                                 -s.maxCorrectionCents, s.maxCorrectionCents);

    st.pitchIn = pitchIn;
    st.target = target;
    st.note = haveNote ? note : -1;
    st.appliedCents = std::isfinite (out) ? out : 0.0;
    return st.appliedCents;
}

} // namespace bmo::tune

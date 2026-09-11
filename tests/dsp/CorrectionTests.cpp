/*
    The correction law (spec §4) and the conversions under it (T-4), driven
    directly with synthetic pitch tracks so each stage can be measured on its
    own: no detector, no engine, the exact input the law sees.

    Output pitch here is "input pitch + applied correction", which is what the
    engine is then asked to produce; CoreTests measures that it does.
*/

#include "modules/tune/dsp/CorrectionLaw.h"
#include "modules/tune/dsp/Pitch.h"
#include "modules/tune/dsp/Scale.h"
#include "tests/TestUtil.h"
#include "tools/common/Signals.h"

#include <cstdio>
#include <functional>
#include <string>

using namespace bmo::tune;
using namespace bmo::tune::test;
namespace sig = bmo::tune::signals;

namespace
{
    constexpr double fs = 48000.0;

    /** Drives a fresh law with a pitch track (Hz per sample; 0 = unvoiced),
        evaluating every `hop` samples as the detector would, and returns the
        output pitch in semitones per sample (input + applied). */
    struct Run
    {
        std::vector<double> out, applied;
        int noteChanges = 0;
    };

    Run drive (const std::function<double (size_t)>& hz, size_t n, const CorrectionSettings& s, int hop = 24)
    {
        CorrectionLaw law;
        law.prepare (fs);
        law.setSettings (s);

        Run r;
        r.out.resize (n);
        r.applied.resize (n);
        PitchEstimate e;
        bool wasVoiced = false;

        for (size_t i = 0; i < n; ++i)
        {
            const auto f = hz (i);
            const auto evaluated = (i % (size_t) hop) == 0;

            if (evaluated)
            {
                e.voiced = f > 0.0;
                e.onset = e.voiced && ! wasVoiced;
                wasVoiced = e.voiced;
                if (e.voiced)
                {
                    e.period = fs / f;
                    e.hz = f;
                }
                e.clarity = e.voiced ? 0.99 : 0.0;
            }
            else
            {
                e.onset = false;
            }

            const auto a = law.tick (e, evaluated);
            r.applied[i] = a;
            r.out[i] = (f > 0.0 ? pitch::semitonesFromHz (f, s.refA) : 0.0) + a / 100.0;
        }

        r.noteChanges = law.state().noteChanges;
        return r;
    }

    double peakToPeakCents (const std::vector<double>& semis, size_t from, size_t to)
    {
        double lo = 1.0e9, hi = -1.0e9;
        for (size_t i = from; i < to; ++i)
        {
            lo = std::min (lo, semis[i]);
            hi = std::max (hi, semis[i]);
        }
        return 100.0 * (hi - lo);
    }

    double meanSemis (const std::vector<double>& semis, size_t from, size_t to)
    {
        double s = 0.0;
        for (size_t i = from; i < to; ++i)
            s += semis[i];
        return s / (double) (to - from);
    }
}

int main()
{
    //== Conversions (T-4) =====================================================
    {
        double worst = 0.0;
        for (auto ref : { 380.0, 415.0, 440.0, 480.0 })
            for (double hz = 30.0; hz < 5000.0; hz *= 1.07)
                worst = std::max (worst, std::abs (pitch::hzFromSemitones (pitch::semitonesFromHz (hz, ref), ref) - hz) / hz);
        check (worst < 1.0e-12, "Hz -> semitones -> Hz round-trips to 1e-12 at every reference from 380 to 480");

        double worstRatio = 0.0;
        for (double c = -1200.0; c <= 1200.0; c += 13.7)
            worstRatio = std::max (worstRatio, std::abs (pitch::centsFromRatio (pitch::ratioFromCents (c)) - c));
        check (worstRatio < 1.0e-9, "cents -> ratio -> cents round-trips");
        check (pitch::semitonesFromHz (480.0, 480.0) == 69.0, "the reference A is note 69 at any reference");
    }

    //== The quantizer (T-4) ===================================================
    {
        int note = 0;
        check (! nearestAllowed (60.3, 0, 0, false, 0.0, note), "an empty scale gives no target");

        const NoteMask onlyA = 1u << 9;
        check (nearestAllowed (63.0, onlyA, 0, false, 0.0, note) && (note == 57 || note == 69),
               "a single-note scale lands on an A in some octave");
        nearestAllowed (66.5, onlyA, 0, false, 0.0, note);
        check (note == 69, "and on the nearest A");

        // Exact 50-cent tie: toward the held note, whichever side it is on.
        nearestAllowed (60.5, kAllNotes, 60, true, 0.0, note);
        check (note == 60, "a 50-cent tie resolves toward the held note below");
        nearestAllowed (60.5, kAllNotes, 61, true, 0.0, note);
        check (note == 61, "and toward the held note above");

        // Hysteresis h: the other note must be h cents *closer*, so the
        // switch point sits h/2 past the midpoint -- 4 cents at the default 8.
        nearestAllowed (60.53, kAllNotes, 60, true, 8.0, note);
        check (note == 60, "3 cents past the midpoint stays on the held note with 8 cents of hysteresis");
        nearestAllowed (60.55, kAllNotes, 60, true, 8.0, note);
        check (note == 61, "5 cents past it moves");

        // Scale masks and keys.
        check (scaleMask (ScaleType::major, 0) == 0b101010110101, "C major is C D E F G A B");
        check (scaleMask (ScaleType::major, 7) == 0b101011010101, "G major is G A B C D E F#");
        check (allows (scaleMask (ScaleType::major, 7), 66) && ! allows (scaleMask (ScaleType::major, 7), 65),
               "G major allows F#4 and not F4");
        check (scaleMask (ScaleType::minor, 9) == scaleMask (ScaleType::major, 0), "A minor has the same notes as C major");
        check (scaleMask (ScaleType::minor, 4) == scaleMask (ScaleType::major, 7), "E minor has the same notes as G major");
        check (scaleMask (ScaleType::minor, 0) == 0b010110101101, "C minor is C D Eb F G Ab Bb");
    }

    //== Retune speed ==========================================================
    std::printf ("retune settling (10-90 %%) on a 40-cent step, vs knob\n");
    {
        check (CorrectionLaw::retuneMsFromKnob (0.0) == 0.0, "retune knob 0 is exactly 0 ms");
        check (near (CorrectionLaw::retuneMsFromKnob (100.0), 400.0, 1.0e-9), "retune knob 100 is 400 ms");

        for (auto knob : { 0.0, 20.0, 50.0, 80.0 })
        {
            CorrectionSettings s;
            s.retuneMs = CorrectionLaw::retuneMsFromKnob (knob);

            // Locked on A4 for 0.2 s, then the singer goes 40 cents sharp.
            const auto step = (size_t) (0.2 * fs);
            const auto r = drive ([step] (size_t i) { return i < step ? 440.0 : 440.0 * std::exp2 (40.0 / 1200.0); },
                                  (size_t) (2.5 * fs), s);

            // Applied goes from 0 to -40 cents; find 10 % and 90 %.
            size_t t10 = 0, t90 = 0;
            for (size_t i = step; i < r.applied.size(); ++i)
            {
                if (t10 == 0 && r.applied[i] <= -4.0) t10 = i;
                if (t90 == 0 && r.applied[i] <= -36.0) { t90 = i; break; }
            }

            const auto ms = 1000.0 * (double) (t90 - t10) / fs;
            char buf[96];
            std::snprintf (buf, sizeof buf, "knob %.0f (tau %.1f ms): 10-90 %% settling", knob, s.retuneMs);
            report (buf, ms, "ms");

            if (knob == 0.0)
                check (t90 > 0 && t90 - step <= 24, "at knob 0 the full correction lands within one evaluation");
            else
                check (t90 > 0 && std::abs (ms - 2.197 * s.retuneMs) < 0.05 * 2.197 * s.retuneMs + 1.0,
                       std::string ("settling is 2.2 tau for a one-pole, knob ") + std::to_string ((int) knob));

            const auto finalOut = r.out.back();
            check (std::abs (100.0 * (finalOut - 69.0)) < 0.01, "and it settles on the note");
        }
    }

    //== Slow glide across a boundary: no flip-flop ============================
    {
        CorrectionSettings s;   // 8 cents of hysteresis, median of 3
        // 2 s from A4 to B4, with +/-3 cents of frame-to-frame jitter on top.
        sig::Random rng (5);
        std::vector<double> jitter ((size_t) (2.0 * fs) / 24 + 2);
        for (auto& j : jitter)
            j = 3.0 * rng.uniform();

        const auto track = [&jitter] (size_t i)
        {
            const auto base = 69.0 + 2.0 * (double) i / (2.0 * fs);
            return pitch::hzFromSemitones (base + jitter[i / 24] / 100.0);
        };

        const auto withHysteresis = drive (track, (size_t) (2.0 * fs), s);
        report ("note changes, A4 -> B4 glide with 3 c jitter", withHysteresis.noteChanges);
        check (withHysteresis.noteChanges == 2, "a slow glide across two boundaries changes note exactly twice");

        s.hysteresisCents = 0.0;
        const auto without = drive (track, (size_t) (2.0 * fs), s);
        report ("the same with hysteresis 0 (median of 3 only)", without.noteChanges);
    }

    //== Vibrato: kept at 100 %, flattened at 0 % (spec §4.3b) =================
    {
        // 330 Hz, 20 cents sharp, 5.5 Hz vibrato of +/-40 cents.
        const auto track = [] (size_t i)
        {
            const auto t = (double) i / fs;
            return 330.0 * std::exp2 ((20.0 + 40.0 * std::sin (2.0 * kPi * 5.5 * t)) / 1200.0);
        };

        const auto from = (size_t) (1.0 * fs), to = (size_t) (2.0 * fs);
        const auto target = pitch::semitonesFromHz (329.63);   // E4

        CorrectionSettings keep;
        keep.vibratoAmount = 1.0;
        const auto k = drive (track, to, keep);
        const auto keptDepth = peakToPeakCents (k.out, from, to);
        const auto keptMean = 100.0 * (meanSemis (k.out, from, to) - target);
        report ("vibrato 100 %: output peak-to-peak (input 80 c)", keptDepth, "c");
        report ("vibrato 100 %: output mean vs E4", keptMean, "c");
        check (keptDepth > 60.0 && keptDepth < 100.0, "vibrato 100 % keeps most of an 80-cent vibrato");
        check (std::abs (keptMean) < 3.0, "and still centres it on the note");

        // At 0 % the vibrato is flattened onto the note. The note decision
        // then follows the raw pitch, so a vibrato that crosses a boundary
        // warbles between the two notes -- the classic hard-tune sound, and
        // the reason the decision moves to the slow pitch as soon as any
        // vibrato is being kept. Both halves are asserted, since each is a
        // choice somebody could undo without noticing.
        const auto inside = [] (size_t i)
        {
            const auto t = (double) i / fs;
            return 330.0 * std::exp2 ((10.0 + 30.0 * std::sin (2.0 * kPi * 5.5 * t)) / 1200.0);
        };

        CorrectionSettings flat;
        flat.vibratoAmount = 0.0;
        const auto f = drive (inside, to, flat);
        const auto flatDepth = peakToPeakCents (f.out, from, to);
        // What is left is a staircase: the detector updates every 0.5 ms and
        // a +/-30 c vibrato at 5.5 Hz moves ~0.5 c per hop, so the flattened
        // output steps by that much either side of the note.
        report ("vibrato 0 %, +10 +/-30 c: output peak-to-peak", flatDepth, "c");
        check (flatDepth < 2.0, "vibrato 0 % flattens a vibrato onto the note, to within a hop's movement");

        const auto crossing = drive (track, to, flat);
        report ("vibrato 0 %, +20 +/-40 c (crosses E/F): note changes", crossing.noteChanges);
        check (crossing.noteChanges > 5, "at 0 % a vibrato crossing a boundary warbles between notes (the effect)");
        check (k.noteChanges == 0, "at 100 % the same vibrato never changes note");
    }

    //== Outliers and leaps never become interval-sized corrections ============
    // The bug the corpus found: a note median paired a held note with a pitch
    // that had already jumped, and the engine was told to shift by the whole
    // interval -- +1200 cents on a one-frame octave error at a note's end.
    {
        CorrectionSettings s;   // chromatic: no correction should ever exceed 50 cents

        // One evaluation an octave low, mid-note.
        const auto blip = (size_t) (0.3 * fs);
        const auto outlier = drive ([blip] (size_t i) { return (i >= blip && i < blip + 24) ? 220.0 : 441.0; },
                                    (size_t) (0.6 * fs), s);
        double worst = 0.0;
        for (auto a : outlier.applied)
            worst = std::max (worst, std::abs (a));
        report ("one-frame octave outlier: largest correction", worst, "c");
        check (worst < 50.0, "a lone octave-error frame never drives more than a chromatic correction");
        check (outlier.noteChanges == 0, "and never changes the note");

        // Real leaps, sung slightly off: every correction stays chromatic-sized.
        for (auto semis : { 5, 7, 12, -12 })
        {
            const auto step = (size_t) (0.3 * fs);
            const auto to = 330.0 * std::exp2 (semis / 12.0) * std::exp2 (15.0 / 1200.0);
            const auto leap = drive ([step, to] (size_t i) { return i < step ? 330.0 * std::exp2 (-10.0 / 1200.0) : to; },
                                     (size_t) (0.6 * fs), s);
            double biggest = 0.0;
            for (auto a : leap.applied)
                biggest = std::max (biggest, std::abs (a));
            char buf[96];
            std::snprintf (buf, sizeof buf, "leap of %+d semitones: largest correction", semis);
            report (buf, biggest, "c");
            check (biggest <= 50.0, std::string ("a real leap is never corrected by its interval: ") + std::to_string (semis));
        }
    }

    //== Glide: HYBRID only (spec §4.5) ========================================
    {
        const auto step = (size_t) (0.2 * fs);
        const auto track = [step] (size_t i) { return i < step ? 440.0 : 493.88; };   // A4 -> B4

        CorrectionSettings s;
        s.glideMs = 50.0;
        s.glideAllowed = true;
        const auto g = drive (track, (size_t) (0.5 * fs), s);
        const auto mid = g.out[step + (size_t) (0.025 * fs)];
        report ("glide 50 ms: target 25 ms into an A4->B4 step", 100.0 * (mid - 69.0), "c");
        check (std::abs (100.0 * (mid - 69.0) - 100.0) < 15.0, "a 50 ms glide is halfway through a whole tone at 25 ms");

        s.glideAllowed = false;   // CLASSIC
        const auto c = drive (track, (size_t) (0.5 * fs), s);
        const auto cmid = c.out[step + (size_t) (0.025 * fs)];
        check (std::abs (100.0 * (cmid - 71.0)) < 1.0, "CLASSIC ignores glide: the target is the new note at once");
    }

    //== Flex knee =============================================================
    {
        check (CorrectionLaw::flexGain (3.0, 0.0) == 1.0, "flex 0 corrects everything");
        check (CorrectionLaw::flexGain (5.0, 1.0) == 0.0, "flex 100 % leaves 5 cents alone");
        check (CorrectionLaw::flexGain (60.0, 1.0) == 1.0, "and corrects 60 cents fully");

        // C1: the applied correction u g(u) has no kink at either end.
        const auto slope = [] (double u) { return (CorrectionLaw::flexGain (u + 1e-6, 1.0) * (u + 1e-6)
                                                  - CorrectionLaw::flexGain (u - 1e-6, 1.0) * (u - 1e-6)) / 2e-6; };
        check (std::abs (slope (50.0 - 1e-4) - slope (50.0 + 1e-4)) < 1e-2, "the knee meets full correction with no kink");
        check (std::abs (slope (20.0 + 1e-4) - slope (20.0 - 1e-4)) < 1e-2, "and leaves the deadzone with no kink");
    }

    //== Every note switched off (T-4, "all-notes-bypassed") ==================
    {
        CorrectionSettings s;
        s.allowed = 0;
        const auto r = drive ([] (size_t) { return 452.0; }, (size_t) (0.5 * fs), s);
        double worst = 0.0;
        for (auto a : r.applied)
            worst = std::max (worst, std::abs (a));
        check (worst == 0.0, "with every note switched off the correction is exactly zero, every sample");
        check (r.noteChanges == 0, "and no note is ever chosen");
    }

    return finish ("correction");
}

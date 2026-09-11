/*
    The HYBRID engine (spec §6) through the whole core.

    The claims:
      - with nothing to correct it is a clean delay, both formant modes
      - at retune 0 a steady note lands within 3 cents of its target (§9)
      - Formant Correct keeps the formants where they were: F1-F3 within 2 %
        across a +/-100-cent correction (§9) -- and CLASSIC, asserted in the
        same place, moves them by exactly the correction, which is its sound
      - Formant Shift moves them by what it says
      - its latency is the shared contract: Live's floor, Studio's figure
      - block-size invariant, and an engine switch mid-note does not click

    Formants are measured by fitting, not by peak-picking: the output's
    harmonic amplitudes are compared against ideal voices synthesised at the
    output pitch with their formants scaled by s, and the best s is where the
    output's formants are. Peak-picking a harmonic spectrum spaced 130 Hz
    apart cannot resolve 2 % of 700 Hz; a fit over thirty harmonics can.
*/

#include "modules/tune/dsp/TuneCore.h"
#include "tests/TestUtil.h"
#include "tools/common/Analysis.h"
#include "tools/common/Signals.h"

#include <complex>
#include <cstdio>
#include <string>

using namespace bmo::tune;
using namespace bmo::tune::test;
namespace sig = bmo::tune::signals;
namespace an = bmo::tune::analysis;

namespace
{
    constexpr double fs = 48000.0;

    std::vector<float> render (const std::vector<float>& in, const TuneParams& p, int block = 128,
                               const std::vector<NoteEvent>& notes = {}, int switchAt = -1, Engine switchTo = Engine::hybrid)
    {
        TuneCore core;
        core.setParams (p);
        core.prepare (fs, 4096);

        auto out = in;
        size_t at = 0, noteIndex = 0;
        sig::Random rng (3);
        bool switched = false;

        while (at < out.size())
        {
            auto n = block > 0 ? block : 1 + (int) (rng.next() % 700);
            n = (int) std::min<size_t> ((size_t) n, out.size() - at);

            if (! switched && switchAt >= 0 && at >= (size_t) switchAt)
            {
                auto q = p;
                q.engine = switchTo;
                core.setParams (q);
                switched = true;
            }

            std::vector<NoteEvent> blockNotes;
            while (noteIndex < notes.size() && (size_t) notes[noteIndex].offset < at + (size_t) n)
            {
                auto e = notes[noteIndex++];
                e.offset = (int) ((size_t) e.offset - at);
                blockNotes.push_back (e);
            }

            core.process (out.data() + at, n, blockNotes.data(), (int) blockNotes.size());
            at += (size_t) n;
        }

        return out;
    }

    TuneParams hybrid (bool formant = true)
    {
        TuneParams p;
        p.engine = Engine::hybrid;
        p.formant = formant;
        return p;
    }

    /** Harmonic amplitudes in dB, Hann-windowed Goertzel at k f0. */
    std::vector<double> profile (const std::vector<float>& x, size_t start, size_t length, double f0, int count)
    {
        std::vector<double> db ((size_t) count);
        for (int k = 1; k <= count; ++k)
        {
            double re = 0.0, im = 0.0;
            const auto w = 2.0 * kPi * k * f0 / fs;
            for (size_t i = 0; i < length; ++i)
            {
                const auto hann = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) (length - 1));
                re += hann * x[start + i] * std::cos (w * (double) i);
                im -= hann * x[start + i] * std::sin (w * (double) i);
            }
            db[(size_t) k - 1] = 10.0 * std::log10 (re * re + im * im + 1.0e-30);
        }
        return db;
    }

    /** RMS dB distance after removing the level difference. */
    double distance (const std::vector<double>& a, const std::vector<double>& b)
    {
        double mean = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            mean += a[i] - b[i];
        mean /= (double) a.size();

        double s = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            const auto d = a[i] - b[i] - mean;
            s += d * d;
        }
        return std::sqrt (s / (double) a.size());
    }

    /** The ideal voice's harmonic amplitudes, in dB, computed rather than
        synthesised: Signals::voice is a 1/k^2 source through a parallel
        bank of two-pole resonators, so harmonic k's level is exactly
        |sum_f w_f g_f / (1 + a1_f z^-1 + a2_f z^-2)| / k^2 at z = e^{j k w0}.
        Synthesising and measuring 80 voices per fit took most of this
        file's 45 seconds and could not reach past a 10 % shift. */
    std::vector<double> idealProfile (double f0, double scale, int count)
    {
        const sig::VoiceSettings v;
        std::vector<double> db ((size_t) count);

        for (int k = 1; k <= count; ++k)
        {
            const auto w = 2.0 * kPi * k * f0 / fs;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
            std::complex<double> h = 0.0;

            for (int f = 0; f < v.numFormants; ++f)
            {
                const auto hz = v.formants[f] * scale, bw = v.bandwidths[f] * scale;
                const auto r = std::exp (-kPi * bw / fs);
                const auto a1 = -2.0 * r * std::cos (2.0 * kPi * hz / fs);
                const auto a2 = r * r;
                const auto g = (1.0 - r) * std::sqrt (1.0 - 2.0 * r * std::cos (4.0 * kPi * hz / fs) + r * r);
                h += (1.0 / (1.0 + f)) * g / (1.0 + a1 * z1 + a2 * z2);
            }

            db[(size_t) k - 1] = 20.0 * std::log10 (std::abs (h) / ((double) k * k) + 1.0e-30);
        }

        return db;
    }

    /** Where the output's formants are, as a scale of the input's: the s
        whose ideal voice at `f0` best matches the output's harmonics. */
    double formantScale (const std::vector<float>& y, double f0, int count = 28)
    {
        const auto measured = profile (y, (size_t) (0.4 * fs), (size_t) (0.4 * fs), f0, count);
        double best = 1.0, bestDistance = 1.0e9;

        for (double s = 0.80; s <= 1.30 + 1e-9; s += 0.001)
        {
            const auto d = distance (measured, idealProfile (f0, s, count));
            if (d < bestDistance) { bestDistance = d; best = s; }
        }

        return best;
    }

    std::string label (const char* what, double a, double b = 0.0)
    {
        char buf[160];
        std::snprintf (buf, sizeof buf, what, a, b);
        return buf;
    }
}

int main()
{
    //== A clean delay when there is nothing to correct ========================
    for (auto formant : { false, true })
    {
        auto p = hybrid (formant);
        p.midiMode = MidiTarget::Mode::target;
        p.midiRequired = true;   // correction exactly zero

        const auto x = sig::voice (sig::steady (196.0, 0.6, fs), fs).samples;
        const auto y = render (x, p);
        const auto d = an::delayOf (x, y, 2000);

        double worst = 0.0, energy = 0.0;
        for (size_t i = (size_t) (0.1 * fs); i < x.size(); ++i)
        {
            const auto e = (double) y[i] - (double) x[i - (size_t) d];
            worst = std::max (worst, std::abs (e));
            energy += (double) x[i] * x[i];
        }

        report (label ("passthrough, formant %.0f: delay", formant ? 1.0 : 0.0), d, "samples");
        report (label ("passthrough, formant %.0f: worst deviation", formant ? 1.0 : 0.0), 20.0 * std::log10 (worst + 1e-30), "dBFS");
        check (d == contract::kLiveRest, label ("HYBRID idles at the Live rest delay (formant %.0f)", formant ? 1.0 : 0.0));
        check (worst < 1.0e-3, label ("and passes the input within -60 dBFS (formant %.0f)", formant ? 1.0 : 0.0));
    }

    //== Tuning accuracy at retune 0 ===========================================
    std::printf ("HYBRID output tuning, retune 0, chromatic\n");
    for (auto formant : { true, false })
        for (auto mode : { LatencyMode::live, LatencyMode::studio })
            for (auto base : { 110.0, 220.0, 440.0, 880.0 })
                for (auto offset : { -40.0, 30.0 })
                {
                    auto p = hybrid (formant);
                    p.latency = mode;
                    const auto hz = base * std::exp2 (offset / 1200.0);
                    const auto y = render (sig::voice (sig::steady (hz, 0.8, fs), fs).samples, p);
                    const auto measured = an::measureHz (y, (size_t) (0.3 * fs), (size_t) (0.4 * fs), fs);
                    const auto err = measured > 0.0 ? an::cents (measured, base) : 1.0e9;

                    char buf[128];
                    std::snprintf (buf, sizeof buf, "%s %s %.0f Hz %+.0f c: output error",
                                   formant ? "fmt" : "raw", mode == LatencyMode::live ? "Live  " : "Studio", base, offset);
                    report (buf, err, "c");
                    check (std::abs (err) < 3.0, std::string (buf) + " under 3 cents");
                }

    //== Formants: HYBRID keeps them, CLASSIC moves them (spec §9, T-3) ========
    std::printf ("formant scale after a +/-100-cent correction (1.000 = unmoved)\n");
    {
        // C3 input, MIDI-steered a semitone either way: a correction a tuner
        // actually applies, on a voice with thirty harmonics under 4 kHz.
        const double f0 = 130.81;
        const auto x = sig::voice (sig::steady (f0, 1.0, fs), fs).samples;
        check (std::abs (formantScale (x, f0) - 1.0) < 0.005, "the fit reads the unprocessed input as unmoved");

        for (auto semis : { -1, 1 })
        {
            const auto outHz = f0 * std::exp2 (semis / 12.0);
            const auto rho = std::exp2 (semis / 12.0);
            const std::vector<NoteEvent> note { { 0, 48 + semis, true } };

            auto h = hybrid (true);
            h.midiMode = MidiTarget::Mode::target;
            const auto sh = formantScale (render (x, h, 128, note), outHz);

            TuneParams c;
            c.midiMode = MidiTarget::Mode::target;
            const auto sc = formantScale (render (x, c, 128, note), outHz);

            report (label ("HYBRID formant on, %+.0f semitone: formant scale", semis), sh);
            report (label ("CLASSIC,           %+.0f semitone: formant scale", semis), sc);
            report (label ("   (the correction ratio is %.4f)", rho), rho);
            check (std::abs (sh - 1.0) < 0.02, label ("HYBRID keeps F1-F3 within 2 %% across %+.0f semitone", semis));
            check (std::abs (sc - rho) < 0.01, label ("CLASSIC moves them by the correction ratio, as designed, %+.0f", semis));
        }

        // Formant Shift: +200 cents on the envelope, pitch left where it is.
        auto h = hybrid (true);
        h.formantShiftCents = 200.0;
        h.midiMode = MidiTarget::Mode::target;
        const auto shifted = formantScale (render (x, h, 128, { { 0, 48, true } }), f0);
        report ("HYBRID formant shift +200 c: formant scale (ratio 1.1225)", shifted);
        check (shifted > 1.08, "Formant Shift +200 cents moves the formants up by most of 12 %");
    }

    //== Latency ===============================================================
    {
        auto p = hybrid (true);
        p.latency = LatencyMode::studio;
        p.midiMode = MidiTarget::Mode::target;
        p.midiRequired = true;

        sig::VoiceSettings fingerprint;
        fingerprint.shimmer = 0.3;
        const auto x = sig::voice (sig::steady (220.0, 0.6, fs), fs, fingerprint).samples;
        const auto measured = an::delayOf (x, render (x, p), 3000);
        const auto reported = TuneCore::latencyFor (p, fs);
        report ("HYBRID Studio: measured delay", measured, "samples");
        report ("HYBRID Studio: reported", reported, "samples");
        check (std::abs (measured - reported) <= 1, "HYBRID Studio measures what it reports (T-5)");

        auto c = p;
        c.engine = Engine::classic;
        check (TuneCore::latencyFor (c, fs) == reported, "both engines report the same Studio latency, so switching never moves PDC");
    }

    //== Block-size invariance =================================================
    {
        auto p = hybrid (true);
        p.retune = 15.0;
        const auto c = sig::concat ({ sig::vibrato (247.0, 70.0, 5.0, 0.6, fs), sig::silence (0.05, fs),
                                      sig::glide (300.0, 150.0, 0.4, fs) });
        const auto x = sig::voice (c, fs).samples;
        const auto reference = render (x, p, 1);
        bool same = true;
        for (auto bs : { 7, 64, 256, 4096, 0 })
            same = same && render (x, p, bs) == reference;
        check (same, "HYBRID is bit-identical at block sizes 1, 7, 64, 256, 4096 and random");
    }

    //== Switching engines mid-note ============================================
    {
        TuneParams p;   // CLASSIC, switching to HYBRID at 0.4 s
        const auto x = sig::voice (sig::steady (233.0, 0.8, fs), fs).samples;
        const auto y = render (x, p, 128, {}, (int) (0.4 * fs), Engine::hybrid);

        // Largest sample-to-sample step around the switch, against the same
        // measure well away from it.
        const auto biggestStep = [&y] (size_t from, size_t to)
        {
            double s = 0.0;
            for (size_t i = from + 1; i < to; ++i)
                s = std::max (s, std::abs ((double) y[i] - (double) y[i - 1]));
            return s;
        };

        const auto around = biggestStep ((size_t) (0.395 * fs), (size_t) (0.45 * fs));
        const auto away = biggestStep ((size_t) (0.2 * fs), (size_t) (0.35 * fs));
        report ("engine switch: largest step around it / away from it", around / away);
        check (around < 1.5 * away, "switching CLASSIC -> HYBRID mid-note adds no step bigger than the note's own");
    }

    return finish ("hybrid");
}

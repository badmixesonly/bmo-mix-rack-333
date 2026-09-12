/*
    Hard tune on a moving voice, and the latency rule -- BMO Tune RT against
    the tuners it competes with, on the reference stimulus
    (tools/common/Stimulus.h), scored by the same code for all of them.

    Why this suite exists: the 2026-09-11 shoot-out on real vocals found BMO
    about as exact as Antares on a held note, and further behind the faster
    the pitch moved -- the error of a correction that lands late. CoreTests
    only holds steady notes, so nothing here saw it. This measures it.

      hardtune           (runs by default)
        - the ruler reads a known delay and a known correction lag
        - THE LATENCY RULE, worst against worst: BMO's true latency is no more
          than Waves Tune Real-Time's, measured the same way (Frosty,
          2026-09-11; AGENTS.md). Necessary, and nowhere near sufficient --
          both figures come from the lowest note in the stimulus.
        - THE LATENCY RULE, PER NOTE: BMO is no later than Waves at EVERY
          marked note. **Open: it fails today**, at A4 and A5, by up to
          3.90 ms. Waves' delay tracks the period and BMO's rest is a
          constant, so which of them is later depends on the note, and the
          worst-against-worst form cannot see it.
        - BMO's correction lag is no worse than the current baseline, while
          the fix is worked on

      hardtune_target    (hardtune_tests --target; disabled in ctest until
                          it passes -- see tests/CMakeLists.txt)
        - BMO flattens a vibrato as closely as Antares does: **passes since
          2026-09-12**, 1.24 c against Antares' 1.30, where it was 3.35.
        - and its worst correction lag is no worse than Antares' worst. Open,
          and the last thing between this file and green: 1.97 ms at A2
          against 1.66 ms. Every other vibrato is inside half a millisecond.

    Both of the remaining open checks come from the same place, found
    2026-09-11: the engine's read delay is a flat 4 ms where the detector's
    analysis lag and Waves' delay are both a period of the note. Predicting
    the pitch forward closed the correction lag (2026-09-12); bounding the
    engine's window is what the per-note latency rule still needs.
    testing-notes/tune-latency-review-2026-09-11.md.
*/

#include "modules/tune/dsp/TuneCore.h"
#include "tests/dsp/tune/TestUtil.h"
#include "tools/tune/common/References.h"
#include "tools/tune/common/Stimulus.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace bmo::tune;
using namespace bmo::tune::test;
namespace st = bmo::tune::stimulus;
namespace sig = bmo::tune::signals;
namespace ref = bmo::tune::references;

namespace
{
    constexpr double fs = 48000.0;

    /** BMO Tune RT's measured numbers, as the regression guard's baseline.

        6.22 ms and 6.61 c until 2026-09-11, which were the figures from
        BEFORE the 4 ms rest landed in 31b30ef. Nobody re-ratcheted it, so the
        guard sat a factor of two slack and would not have noticed the rest
        being reverted -- exactly the regression it exists to catch. Move them
        down with any change that improves them, and say so.

        Now the figures with the prediction in: the estimate carried forward
        to where the engine reads, which is what the lag was. */
    constexpr double kBaselineMeanLagMs = 0.71;
    constexpr double kBaselineRmsCents = 1.24;

    std::vector<float> renderBmo (const std::vector<float>& in)
    {
        TuneCore core;
        core.setParams (TuneParams {});   // chromatic, 0.0 ms, vibrato 0, Auto
        core.prepare (fs, 128);
        auto y = in;
        for (size_t at = 0; at < y.size(); at += 128)
            core.process (y.data() + at, (int) std::min<size_t> (128, y.size() - at));
        return y;
    }

    /** What an ideal hard-tune corrector that is `lagMs` late and whose
        audio is `delayMs` late would put out: in-tune and marked segments are
        the input delayed (the marked ones moved onto their note, with their
        level dips kept), and each vibrato is flattened but for
        c(t) - c(t - lag). */
    std::vector<float> idealCorrector (const st::Stimulus& s, double lagMs, double delayMs)
    {
        const auto d = (size_t) std::lround (delayMs * 0.001 * fs);
        std::vector<float> out (s.samples.size() + d, 0.0f);

        for (const auto& seg : s.segments)
        {
            std::vector<float> x;

            if (seg.kind == st::Kind::inTune)
            {
                x.assign (s.samples.begin() + (long) seg.start, s.samples.begin() + (long) (seg.start + seg.length));
            }
            else if (seg.kind == st::Kind::marked)
            {
                sig::VoiceSettings v;
                v.seed = seg.seed + 500;   // a different voice: only the dips are shared
                x = sig::voice (sig::steady (seg.hz, seg.seconds, fs), fs, v).samples;
                const auto g = st::markers (x.size(), fs, seg.seed);
                for (size_t i = 0; i < x.size(); ++i)
                    x[i] *= g[i];
            }
            else
            {
                sig::Contour c (seg.length);
                for (size_t i = 0; i < c.size(); ++i)
                {
                    const auto t = (double) i / fs;
                    const auto now = seg.depthCents * std::sin (2.0 * sig::kPi * seg.rateHz * t);
                    const auto then = seg.depthCents * std::sin (2.0 * sig::kPi * seg.rateHz * (t - 0.001 * lagMs));
                    c[i] = seg.hz * std::exp2 ((now - then) / 1200.0);
                }
                sig::VoiceSettings v;
                v.seed = seg.seed;
                x = sig::voice (c, fs, v).samples;
            }

            for (size_t i = 0; i < x.size() && seg.start + d + i < out.size(); ++i)
                out[seg.start + d + i] = x[i];
        }

        out.resize (s.samples.size());
        return out;
    }

    void reportScore (const std::string& who, const st::Score& sc)
    {
        for (const auto& r : sc.rows)
        {
            if (r.kind == st::Kind::vibrato)
            {
                report (who + ": " + r.name + ", correction lag", r.lagMs, "ms");
                report (who + ": " + r.name + ", RMS off the note", r.rmsCents, "c");
            }
            else
            {
                report (who + ": " + r.name + ", delay", r.delayMs, "ms");
            }
        }
        report (who + ": true latency", sc.trueLatencyMs, "ms");
        report (who + ": correction lag, mean", sc.meanLagMs, "ms");
        report (who + ": RMS off the note, mean", sc.meanRmsCents, "c");
    }
}

int main (int argc, char** argv)
{
    const bool target = argc > 1 && std::strcmp (argv[1], "--target") == 0;
    const auto s = st::make (fs);

    //== The ruler is a ruler =================================================
    {
        // A plain 2.5 ms delay reads 2.5 ms on every segment it is read on.
        const auto d = (size_t) std::lround (0.0025 * fs);
        std::vector<float> delayed (s.samples.size(), 0.0f);
        std::copy (s.samples.begin(), s.samples.end() - (long) d, delayed.begin() + (long) d);

        bool all = true;
        for (const auto& r : st::score (s, delayed).rows)
            if (r.kind != st::Kind::vibrato)
                all = all && std::abs (r.delayMs - 2.5) < 0.05;
        check (all, "a plain 2.5 ms delay reads 2.5 ms, within 0.05, on every in-tune and marked segment");
    }

    for (const auto& [lag, delay] : { std::pair { 0.0, 0.0 }, std::pair { 5.0, 0.0 }, std::pair { 2.0, 3.0 } })
    {
        // The delay tolerance scales with the period, and is not a flat
        // 0.25 ms as it was until 2026-09-11.
        //
        // On a marked segment the ideal corrector is a DIFFERENT voice at the
        // target pitch sharing only the marker pattern, so what limits the
        // envelope correlation is how much of the two voices' own amplitude
        // variation survives the smoothing -- and that is a number of periods,
        // not a number of milliseconds. A flat tolerance is therefore the
        // wrong shape: it was slack at A5 by a factor of seven and just tight
        // enough at E2 to fail (0.334 ms) a ruler that is working correctly.
        // 3 % of the period, floored at the old 0.25 ms so nothing above
        // ~250 Hz is loosened. testing-notes/tune-latency-review-2026-09-11.md.
        const auto toleranceFor = [] (double hz) { return std::max (0.25, 0.03 * 1000.0 / hz); };

        const auto sc = st::score (s, idealCorrector (s, lag, delay));
        double worstLagErr = 0.0, worstDelayRatio = 0.0, worstDelayErr = 0.0;
        std::string worstDelayAt;
        for (size_t k = 0; k < sc.rows.size(); ++k)
        {
            const auto& r = sc.rows[k];
            if (r.kind == st::Kind::vibrato)
            {
                worstLagErr = std::max (worstLagErr, std::abs (r.lagMs - lag));
                continue;
            }

            double hz = 0.0;
            for (const auto& seg : s.segments)
                if (r.name == seg.name)
                    hz = seg.hz;

            const auto err = std::abs (r.delayMs - delay);
            const auto ratio = err / toleranceFor (hz);

            if (ratio > worstDelayRatio)
            {
                // Named, so a ruler failure says which segment rather than
                // only how far out: the segment is the diagnosis.
                worstDelayRatio = ratio;
                worstDelayErr = err;
                worstDelayAt = r.name;
            }
        }

        char what[160];
        std::snprintf (what, sizeof what, "an ideal corrector %.0f ms late with its audio %.0f ms late", lag, delay);
        report (std::string (what) + ": worst lag error", worstLagErr, "ms");
        report (std::string (what) + ": worst delay error, on " + worstDelayAt, worstDelayErr, "ms");
        report (std::string (what) + ": ...as a share of that segment's tolerance", worstDelayRatio, "x");
        check (worstLagErr < 0.3, std::string (what) + " reads its lag within 0.3 ms on every vibrato");
        check (worstDelayRatio < 1.0,
               std::string (what) + " reads its delay within 3 % of a period (min 0.25 ms) on every "
                                    "in-tune and marked segment");
    }

    //== BMO Tune RT ===========================================================
    const auto bmo = st::score (s, renderBmo (s.samples));
    reportScore ("BMO", bmo);

    // THE LATENCY RULE (Frosty, 2026-09-11): a change is safe to take, as far
    // as latency goes, while BMO's true latency stays no more than Waves Tune
    // Real-Time's, measured the same way on the same stimulus. Not what the
    // host is told -- both say 0 -- but how late the audio really is.
    //
    // Worst against worst, which is what the rule says and is necessary but
    // nowhere near sufficient: both figures are dominated by the lowest note
    // in the stimulus, where Waves is 19.2 ms and BMO 9.2, so this passes with
    // 10 ms to spare while BMO is later than Waves over most of the range.
    // The per-note check below is the one that means anything.
    report ("Waves Tune Real-Time: true latency (the ceiling)", ref::kWaves.trueLatencyMs, "ms");
    report ("headroom under the ceiling", ref::kWaves.trueLatencyMs - bmo.trueLatencyMs, "ms");
    check (bmo.trueLatencyMs <= ref::kWaves.trueLatencyMs,
           "BMO's true latency is no more than Waves Tune Real-Time's, worst against worst (the latency rule)");

    // THE LATENCY RULE, PER NOTE. Waves' delay while correcting is nearly
    // proportional to the period (1.68 ms per ms of it) and BMO's rest is a
    // constant 4 ms, so which of the two is later depends entirely on the
    // note, and a single worst-case comparison cannot see it. Measured
    // 2026-09-11: BMO is under Waves at E2, A2, D3 and A3, and over it at A4
    // (5.01 against 3.82) and A5 (4.61 against 0.71).
    //
    // THIS FAILS TODAY, by design -- it is the rule stated honestly against an
    // engine that does not yet keep it, and it is the test the fix has to turn
    // green. The cause is `hi = rest + T` in ClassicEngine plus a rest that
    // does not track the period; the same root cause as the correction lag.
    // testing-notes/tune-latency-review-2026-09-11.md.
    {
        int over = 0;
        double worstBy = 0.0;

        for (const auto& r : bmo.rows)
        {
            if (r.kind != st::Kind::marked || ! std::isfinite (r.delayMs))
                continue;

            double hz = 0.0;
            for (const auto& seg : s.segments)
                if (r.name == seg.name)
                    hz = seg.hz;

            const auto ceiling = ref::ceilingMsAt (hz);
            report ("BMO vs Waves at " + r.name + ": BMO " + std::to_string (r.delayMs).substr (0, 5)
                        + " ms, Waves (the ceiling here)", ceiling, "ms");

            if (r.delayMs > ceiling)
            {
                ++over;
                worstBy = std::max (worstBy, r.delayMs - ceiling);
            }
        }

        report ("segments where BMO is later than Waves at the same note", (double) over, "");
        report ("...worst by", worstBy, "ms");
        check (over == 0,
               "BMO is no later than Waves Tune Real-Time AT EVERY NOTE, not only at the worst one "
               "(the latency rule, per note -- open, fails today)");
    }

    check (bmo.meanLagMs <= kBaselineMeanLagMs * 1.05 && bmo.meanRmsCents <= kBaselineRmsCents * 1.05,
           "BMO's correction lag and vibrato residue are no worse than the 2026-09-11 baseline");

    //== The target: as close as Antares =======================================
    report ("Antares Auto-Tune Artist: RMS off the note, mean", ref::kAntares.meanRmsCents, "c");
    report ("Antares Auto-Tune Artist: worst correction lag", ref::kAntares.worstLagMs, "ms");

    if (target)
    {
        check (bmo.meanRmsCents <= ref::kAntares.meanRmsCents,
               "BMO flattens a vibrato at 0 ms as closely as Antares Auto-Tune Artist (mean RMS off the note)");
        check (bmo.worstLagMs <= ref::kAntares.worstLagMs,
               "and its worst correction lag is no worse than Antares'");
    }

    return finish (target ? "hardtune target" : "hardtune");
}

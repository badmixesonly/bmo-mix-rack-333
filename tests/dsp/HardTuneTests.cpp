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
        - THE LATENCY RULE: BMO's true latency is no more than Waves Tune
          Real-Time's, measured the same way (Frosty, 2026-09-11; AGENTS.md)
        - BMO's correction lag is no worse than on 2026-09-11, while the fix
          is worked on

      hardtune_target    (hardtune_tests --target; disabled in ctest until
                          it passes -- see tests/CMakeLists.txt)
        - BMO flattens a vibrato as closely as Antares does, and its worst
          correction lag is no worse than Antares' worst. Open: it fails today.
*/

#include "modules/tune/dsp/TuneCore.h"
#include "tests/TestUtil.h"
#include "tools/common/References.h"
#include "tools/common/Stimulus.h"

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

    /** BMO Tune RT's measured numbers on 2026-09-11, before any fix: the
        regression guard's baseline. */
    constexpr double kBaselineMeanLagMs = 6.22;
    constexpr double kBaselineRmsCents = 6.61;

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
        const auto sc = st::score (s, idealCorrector (s, lag, delay));
        double worstLagErr = 0.0, worstDelayErr = 0.0;
        for (const auto& r : sc.rows)
        {
            if (r.kind == st::Kind::vibrato) worstLagErr = std::max (worstLagErr, std::abs (r.lagMs - lag));
            else                             worstDelayErr = std::max (worstDelayErr, std::abs (r.delayMs - delay));
        }

        char what[160];
        std::snprintf (what, sizeof what, "an ideal corrector %.0f ms late with its audio %.0f ms late", lag, delay);
        report (std::string (what) + ": worst lag error", worstLagErr, "ms");
        report (std::string (what) + ": worst delay error", worstDelayErr, "ms");
        check (worstLagErr < 0.3, std::string (what) + " reads its lag within 0.3 ms on every vibrato");
        check (worstDelayErr < 0.25, std::string (what) + " reads its delay within 0.25 ms, on pitch-moved segments too");
    }

    //== BMO Tune RT ===========================================================
    const auto bmo = st::score (s, renderBmo (s.samples));
    reportScore ("BMO", bmo);

    // THE LATENCY RULE (Frosty, 2026-09-11): a change is safe to take, as far
    // as latency goes, while BMO's true latency stays no more than Waves Tune
    // Real-Time's, measured the same way on the same stimulus. Not what the
    // host is told -- both say 0 -- but how late the audio really is.
    report ("Waves Tune Real-Time: true latency (the ceiling)", ref::kWaves.trueLatencyMs, "ms");
    report ("headroom under the ceiling", ref::kWaves.trueLatencyMs - bmo.trueLatencyMs, "ms");
    check (bmo.trueLatencyMs <= ref::kWaves.trueLatencyMs,
           "BMO's true latency is no more than Waves Tune Real-Time's (the latency rule)");

    check (bmo.meanLagMs <= kBaselineMeanLagMs * 1.05 && bmo.meanRmsCents <= kBaselineRmsCents * 1.05,
           "BMO's correction lag and vibrato residue are no worse than on 2026-09-11 (6.22 ms, 6.61 c)");

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

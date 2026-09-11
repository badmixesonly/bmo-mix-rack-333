/*
    bmo-tune-latency: the latency table (spec T-5) -- the one that goes in the
    manual, so it is measured, never computed from what the code says it does.

        bmo-tune-latency [--rate 48000] [--engine Classic|Hybrid] [--range Auto|...|all]
                         [--md reports/latency.md] [--csv reports/latency.csv]

    For every semitone of the range, on the synthetic voice:

      rest       delay of an in-tune note through the core, by cross-
                 correlation against the dry signal. The algorithmic floor:
                 what the plugin costs when it is not correcting.
      lock       time from a note's onset (out of silence) to the first
                 detector estimate within 20 cents.
      correcting the engine's read delay while holding a note 35 cents sharp
                 at retune 0 -- mean and worst over the steady state. In Live
                 this wanders up to a period above rest; in Studio it stays
                 within half a period either side of the reported figure.
      reported   what the host is told.

    Both latency contracts are measured. The check at the end holds Studio's
    reported figure to its measured rest delay within one sample -- the
    assertion spec T-5 asks for.
*/

#include "modules/tune/dsp/TuneCore.h"
#include "tools/common/Analysis.h"
#include "tools/common/Params.h"
#include "tools/common/Signals.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace bmo::tune;
namespace sig = bmo::tune::signals;
namespace an = bmo::tune::analysis;

namespace
{
    const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    struct LagStats { double mean = 0.0, worst = 0.0, least = 1.0e9; double lockMs = -1.0; };

    struct Collector
    {
        long long from = 0, onset = 0;
        double targetHz = 0.0;
        LagStats stats;
        double sum = 0.0;
        long long count = 0;

        static void tap (void* context, const AnalysisFrame& f)
        {
            auto* c = static_cast<Collector*> (context);

            if (c->stats.lockMs < 0.0 && f.evaluated && f.voiced && f.sample >= c->onset && f.f0 > 0.0
                && std::abs (an::cents (f.f0, c->targetHz)) < 20.0)
                c->stats.lockMs = 1000.0 * (double) (f.sample - c->onset);   // scaled by fs later

            if (f.sample >= c->from)
            {
                c->sum += f.lag;
                ++c->count;
                c->stats.worst = std::max (c->stats.worst, f.lag);
                c->stats.least = std::min (c->stats.least, f.lag);
            }
        }
    };

    std::vector<float> run (const std::vector<float>& x, const TuneParams& p, double fs, Collector* collector)
    {
        TuneCore core;
        core.setParams (p);
        core.prepare (fs, 512);
        if (collector)
            core.setAnalysisTap (&Collector::tap, collector);

        auto y = x;
        for (size_t at = 0; at < y.size(); at += 256)
            core.process (y.data() + at, (int) std::min<size_t> (256, y.size() - at));
        return y;
    }

    struct Row
    {
        int noteNumber = 0;
        double hz = 0.0, restMs = 0.0, lockMs = 0.0, meanMs = 0.0, worstMs = 0.0, leastMs = 0.0, reportedMs = 0.0;
    };
}

int main (int argc, char** argv)
{
    double fs = 48000.0;
    std::string engineName = "Classic", rangeName = "Auto", mdPath, csvPath;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--rate" && i + 1 < argc)        fs = std::atof (argv[++i]);
        else if (a == "--engine" && i + 1 < argc) engineName = argv[++i];
        else if (a == "--range" && i + 1 < argc)  rangeName = argv[++i];
        else if (a == "--md" && i + 1 < argc)     mdPath = argv[++i];
        else if (a == "--csv" && i + 1 < argc)    csvPath = argv[++i];
        else
        {
            std::fprintf (stderr, "usage: bmo-tune-latency [--rate hz] [--engine Classic|Hybrid] [--range name|all]\n"
                                  "                        [--md out.md] [--csv out.csv]\n");
            return 2;
        }
    }

    const auto& rangeSpec = specs()[(size_t) Index::range];
    std::vector<int> ranges;
    for (int r = 0; r < rangeSpec.numChoices(); ++r)
        if (tools::lower (rangeName) == "all" || tools::lower (rangeSpec.choices[(size_t) r]) == tools::lower (rangeName))
            ranges.push_back (r);

    if (ranges.empty())
    {
        std::fprintf (stderr, "bmo-tune-latency: unknown range %s\n", rangeName.c_str());
        return 2;
    }

    std::string md, csv = "engine,range,mode,note,hz,rest_ms,lock_ms,correcting_mean_ms,correcting_worst_ms,correcting_least_ms,reported_ms\n";
    int failures = 0;
    char line[512];

    for (auto r : ranges)
    {
        for (auto mode : { LatencyMode::live, LatencyMode::studio })
        {
            auto values = tools::defaultValues();
            std::string error;
            tools::setFromText (values, "engine", engineName, error);
            values[(size_t) Index::range] = (float) r;
            values[(size_t) Index::latency] = mode == LatencyMode::studio ? 1.0f : 0.0f;
            const auto params = tools::toParams (values);
            const auto limits = limitsOf (params.range);
            const auto reported = TuneCore::latencyFor (params, fs);
            const char* modeName = mode == LatencyMode::live ? "Live" : "Studio";

            std::snprintf (line, sizeof line, "\n### %s, %s range (%.0f-%.0f Hz), %s, %.1f kHz\n\n"
                           "| note | Hz | rest ms | lock ms | correcting mean ms | correcting worst ms | reported ms |\n"
                           "|---|---:|---:|---:|---:|---:|---:|\n",
                           engineName.c_str(), rangeSpec.choices[(size_t) r], limits.minHz, limits.maxHz, modeName, fs / 1000.0);
            md += line;

            const auto lowest = (int) std::ceil (69.0 + 12.0 * std::log2 (limits.minHz / 440.0));
            const auto highest = (int) std::floor (69.0 + 12.0 * std::log2 (limits.maxHz / 440.0));

            double worstRest = 0.0;

            for (int noteNumber = lowest; noteNumber <= highest; ++noteNumber)
            {
                const auto hz = 440.0 * std::exp2 ((noteNumber - 69) / 12.0);
                Row row;
                row.noteNumber = noteNumber;
                row.hz = hz;
                row.reportedMs = 1000.0 * reported / fs;

                // Rest: correction disabled, analysis path live (spec T-5) --
                // every note switched off gives the quantizer nothing to aim at,
                // so the correction is exactly zero while the detector runs.
                // Two things the first runs got wrong, both recorded so nobody
                // undoes them:
                //
                //  - A steady tone correlates with itself at every whole
                //    period, so on a plain one the rig picked an arbitrary
                //    peak (Studio "measured" 0.6 to 5.2 ms against 8.2). 30 %
                //    random shimmer gives each cycle its own level and only
                //    the true delay lines up.
                //  - With correction left on, that shimmer biases the
                //    detector a few cents, the engine really corrects it, and
                //    the read wanders: the floor is not what was measured.
                {
                    sig::VoiceSettings fingerprint;
                    fingerprint.shimmer = 0.3;
                    fingerprint.seed = 1000u + (unsigned) noteNumber;
                    const auto x = sig::voice (sig::steady (hz, 0.5, fs), fs, fingerprint).samples;

                    auto idle = params;
                    idle.allowed = 0;
                    const auto y = run (x, idle, fs, nullptr);
                    row.restMs = 1000.0 * an::delayOf (x, y, (int) (0.03 * fs)) / fs;
                    worstRest = std::max (worstRest, row.restMs);
                }

                // Lock and correcting lag: 100 ms of silence, then 35 cents sharp.
                {
                    const auto c = sig::concat ({ sig::silence (0.1, fs), sig::steady (hz * std::exp2 (35.0 / 1200.0), 0.6, fs) });
                    Collector col;
                    col.onset = (long long) (0.1 * fs);
                    col.from = (long long) (0.3 * fs);
                    col.targetHz = hz * std::exp2 (35.0 / 1200.0);
                    run (sig::voice (c, fs).samples, params, fs, &col);

                    row.lockMs = col.stats.lockMs >= 0.0 ? col.stats.lockMs / fs : -1.0;
                    row.meanMs = col.count ? 1000.0 * (col.sum / (double) col.count) / fs : 0.0;
                    row.worstMs = 1000.0 * col.stats.worst / fs;
                    row.leastMs = col.count ? 1000.0 * col.stats.least / fs : 0.0;
                }

                std::snprintf (line, sizeof line, "| %s%d | %.1f | %.2f | %.2f | %.2f | %.2f | %.2f |\n",
                               kNoteNames[noteNumber % 12], noteNumber / 12 - 1, hz, row.restMs, row.lockMs, row.meanMs, row.worstMs, row.reportedMs);
                md += line;

                std::snprintf (line, sizeof line, "%s,%s,%s,%s%d,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                               engineName.c_str(), rangeSpec.choices[(size_t) r], modeName, kNoteNames[noteNumber % 12], noteNumber / 12 - 1,
                               hz, row.restMs, row.lockMs, row.meanMs, row.worstMs, row.leastMs, row.reportedMs);
                csv += line;

                if (mode == LatencyMode::studio && std::abs (row.restMs - row.reportedMs) > 1000.0 / fs + 1e-9)
                {
                    std::fprintf (stderr, "FAIL: %s %s Studio at %.1f Hz: measured %.3f ms, reported %.3f ms\n",
                                  engineName.c_str(), rangeSpec.choices[(size_t) r], hz, row.restMs, row.reportedMs);
                    ++failures;
                }
            }

            std::fprintf (stderr, "%s %s %s: rest delay %.3f ms worst, reported %.3f ms\n",
                          engineName.c_str(), rangeSpec.choices[(size_t) r], modeName, worstRest, 1000.0 * reported / fs);
        }
    }

    std::printf ("%s", md.c_str());

    if (! mdPath.empty())
        if (auto* f = std::fopen (mdPath.c_str(), "w")) { std::fputs (md.c_str(), f); std::fclose (f); }
    if (! csvPath.empty())
        if (auto* f = std::fopen (csvPath.c_str(), "w")) { std::fputs (csv.c_str(), f); std::fclose (f); }

    std::fprintf (stderr, failures ? "bmo-tune-latency: %d Studio cell(s) off their reported PDC\n"
                                   : "bmo-tune-latency: Studio PDC matches measurement in every cell\n", failures);
    return failures ? 1 : 0;
}

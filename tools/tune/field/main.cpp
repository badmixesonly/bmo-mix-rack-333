/*
    bmo-tune-field: the hiccup numbers for a real take -- what the 2026-09-11
    shoot-out was diagnosed with, in one command, so any change can be held
    to the real vocals it was made for and not only to the synthetic corpus.

        bmo-tune-field dry.wav [--set id=value ...] [--ruler-min 60] [--ruler-max 400]
                       [--out render.wav] [--csv detector.csv]

    Renders the dry through BMO Tune RT's core, as bmo-tune-cli does, with
    the analysis tap on, and reports:

      detector    every 10 ms frame where the offline ruler (Analysis.h, 40 ms,
                  kept to --ruler-min..--ruler-max, a low male voice by
                  default) finds a pitch, BMO's own estimates in the middle
                  10 ms of that frame, median, sorted: on the note (within
                  60 c), an octave up, a twelfth up, an octave down, other,
                  or BMO unvoiced. An octave keeps the note name and so the
                  correction; a harmonic above means a splice cut mid-cycle,
                  and a twelfth aims at the wrong note name -- those two are
                  the ones heard.
      flips       note changes that change the note NAME, and of those, the
                  ones that come straight back within 80 ms, split into
                  neighbours (1-2 semitones: the note decision) and jumps
                  (the detector).
      dropouts    gaps under 80 ms between voiced stretches: the correction
                  letting go mid-phrase.
      splices     the engine's whole-period jumps.

    The ruler must be kept to the voice's range: a voice whose fundamental
    sits under its second harmonic fools an unbounded ruler as it fooled the
    detector (Failure at 1.00 s reads 606 Hz full-range, 303 Hz bounded).
    Field audio never enters the repository; see HANDOFF.md.
*/

#include "modules/tune/dsp/TuneCore.h"
#include "tools/tune/common/Analysis.h"
#include "tools/tune/common/Params.h"
#include "tools/tune/common/Wav.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace bmo::tune;

namespace
{
    struct Row { long long n; double f0, pitchIn; bool voiced; int note; bool splice, evaluated; };

    struct Collector
    {
        std::vector<Row> rows;

        static void tap (void* context, const AnalysisFrame& f)
        {
            if (f.evaluated || f.splice)
                static_cast<Collector*> (context)->rows.push_back (
                    { f.sample, f.f0, f.pitchIn, f.voiced, f.note, f.splice, f.evaluated });
        }
    };

    int usage()
    {
        std::fprintf (stderr, "usage: bmo-tune-field dry.wav [--set id=value ...] [--ruler-min hz] [--ruler-max hz]\n"
                              "                      [--out render.wav] [--csv detector.csv]\n");
        return 2;
    }
}

int main (int argc, char** argv)
{
    if (argc < 2)
        return usage();

    const std::string inPath = argv[1];
    std::string outPath, csvPath;
    double rulerMin = 60.0, rulerMax = 400.0;
    auto values = tools::defaultValues();

    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto next = [&] { return i + 1 < argc ? std::string (argv[++i]) : std::string(); };
        std::string error;

        if (a == "--set")            { if (! tools::applyAssignment (values, next(), error)) { std::fprintf (stderr, "%s\n", error.c_str()); return 2; } }
        else if (a == "--ruler-min") rulerMin = std::atof (next().c_str());
        else if (a == "--ruler-max") rulerMax = std::atof (next().c_str());
        else if (a == "--out")       outPath = next();
        else if (a == "--csv")       csvPath = next();
        else                         return usage();
    }

    wav::Channels in;
    double fs = 0.0;
    if (! wav::read (inPath, in, fs))
    {
        std::fprintf (stderr, "cannot read %s\n", inPath.c_str());
        return 1;
    }

    // Mono, as the CLI takes it: the channels averaged.
    std::vector<float> dry (in.front().size(), 0.0f);
    for (const auto& ch : in)
        for (size_t i = 0; i < dry.size(); ++i)
            dry[i] += ch[i] / (float) in.size();

    // Render, with the tap.
    Collector col;
    TuneCore core;
    core.setParams (tools::toParams (values));
    core.prepare (fs, 128);
    core.setAnalysisTap (&Collector::tap, &col);
    auto y = dry;
    for (size_t at = 0; at < y.size(); at += 128)
        core.process (y.data() + at, (int) std::min<size_t> (128, y.size() - at));

    if (! outPath.empty())
        wav::writeMono (outPath, y, fs);

    std::vector<Row> ev;
    int splices = 0;
    for (const auto& r : col.rows)
    {
        if (r.splice) ++splices;
        if (r.evaluated) ev.push_back (r);
    }

    if (! csvPath.empty())
        if (auto* f = std::fopen (csvPath.c_str(), "w"))
        {
            std::fprintf (f, "seconds,f0,pitch_in,voiced,note\n");
            for (const auto& r : ev)
                std::fprintf (f, "%.5f,%.3f,%.4f,%d,%d\n", (double) r.n / fs, r.f0, r.pitchIn, r.voiced ? 1 : 0, r.note);
            std::fclose (f);
        }

    //== Detector against the ruler ==========================================
    const auto hop = (size_t) std::lround (0.010 * fs), len = (size_t) std::lround (0.040 * fs);
    int frames = 0, right = 0, up8 = 0, up12 = 0, down8 = 0, other = 0, unvoiced = 0;
    size_t k = 0;

    for (size_t a = 0; a + len < dry.size(); a += hop)
    {
        double e = 0.0;
        for (size_t i = a; i < a + len; ++i) e += (double) dry[i] * dry[i];
        if (10.0 * std::log10 (e / (double) len + 1.0e-20) < -45.0)
            continue;

        const auto truth = analysis::measureHz (dry, a, len, fs, rulerMin, rulerMax);
        if (truth <= 0.0)
            continue;

        const double lo = (double) a + 0.015 * fs, hi = (double) a + 0.025 * fs;
        while (k < ev.size() && (double) ev[k].n < lo) ++k;
        std::vector<double> est;
        for (size_t j = k; j < ev.size() && (double) ev[j].n < hi; ++j)
            if (ev[j].voiced && ev[j].f0 > 0.0)
                est.push_back (ev[j].f0);

        ++frames;
        if (est.empty()) { ++unvoiced; continue; }

        std::sort (est.begin(), est.end());
        const auto c = 1200.0 * std::log2 (est[est.size() / 2] / truth);
        if (std::abs (c) < 60.0)               ++right;
        else if (std::abs (c - 1200.0) < 80.0) ++up8;
        else if (std::abs (c - 1902.0) < 80.0) ++up12;
        else if (std::abs (c + 1200.0) < 80.0) ++down8;
        else                                   ++other;
    }

    //== Note-name flips =======================================================
    struct Change { long long n; int from, to; };
    std::vector<Change> changes;
    int prev = -1;
    bool prevVoiced = false;
    for (const auto& r : ev)
    {
        if (! r.voiced || r.note < 0) { prevVoiced = false; continue; }
        if (prevVoiced && prev >= 0 && r.note != prev && ((r.note - prev) % 12) != 0)
            changes.push_back ({ r.n, prev, r.note });
        prev = r.note;
        prevVoiced = true;
    }

    int flips = 0, neighbour = 0, jump = 0;
    for (size_t c = 1; c < changes.size(); ++c)
        if (changes[c].to == changes[c - 1].from && (double) (changes[c].n - changes[c - 1].n) / fs < 0.080)
        {
            ++flips;
            (std::abs (changes[c].to - changes[c].from) <= 2 ? neighbour : jump)++;
        }

    //== Dropouts ==============================================================
    int dropouts = 0;
    long long lastVoiced = -1;
    bool inRun = false;
    for (const auto& r : ev)
    {
        if (r.voiced)
        {
            if (! inRun && lastVoiced >= 0 && (double) (r.n - lastVoiced) / fs < 0.080)
                ++dropouts;
            inRun = true;
            lastVoiced = r.n;
        }
        else
        {
            inRun = false;
        }
    }

    const auto pct = [frames] (int v) { return frames ? 100.0 * v / frames : 0.0; };
    const auto seconds = (double) dry.size() / fs;
    std::printf ("%s  (%.1f s, %.0f Hz; ruler %.0f-%.0f Hz)\n", inPath.c_str(), seconds, fs, rulerMin, rulerMax);
    std::printf ("  detector, %d voiced frames: on the note %.1f %% | octave up %.1f %% | twelfth up %.1f %% | "
                 "octave down %.1f %% | other %.1f %% | BMO unvoiced %.1f %%\n",
                 frames, pct (right), pct (up8), pct (up12), pct (down8), pct (other), pct (unvoiced));
    std::printf ("  note-name changes %zu; flips back within 80 ms %d (neighbours %d, jumps %d)\n",
                 changes.size(), flips, neighbour, jump);
    std::printf ("  dropouts under 80 ms %d | splices %d (%.1f/s)\n", dropouts, splices, splices / seconds);
    return 0;
}

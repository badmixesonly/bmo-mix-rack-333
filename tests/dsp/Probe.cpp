// Scratch diagnostic, not a test: where does a core render deviate from the
// delayed input, and what was the engine doing there.
#include "modules/tune/dsp/TuneCore.h"
#include "tools/common/Signals.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace bmo::tune;
namespace sig = bmo::tune::signals;

struct Ctx { std::vector<AnalysisFrame> frames; };

int main (int argc, char** argv)
{
    const double fs = 48000.0;
    const double hz = argc > 1 ? std::atof (argv[1]) : 440.0;
    const bool required = argc > 2 && std::atoi (argv[2]) != 0;

    TuneParams p;
    if (required) { p.midiMode = MidiTarget::Mode::target; p.midiRequired = true; }

    TuneCore core;
    core.setParams (p);
    core.prepare (fs, 4096);
    Ctx ctx;
    core.setAnalysisTap ([] (void* c, const AnalysisFrame& f) { ((Ctx*) c)->frames.push_back (f); }, &ctx);

    const auto x = sig::sine (sig::steady (hz, 0.5, fs), fs, 0.8).samples;
    auto y = x;
    for (size_t at = 0; at < y.size(); at += 128)
        core.process (y.data() + at, (int) std::min<size_t> (128, y.size() - at));

    const int d = ClassicEngine::kLiveRest;
    int printed = 0;
    for (size_t i = (size_t) d; i < x.size() && printed < 25; ++i)
    {
        const auto dev = std::abs (y[i] - x[i - (size_t) d]);
        if (dev > 1.0e-6f)
        {
            const auto& f = ctx.frames[i];
            std::printf ("n=%6zu dev=%.3g y=%.6f x=%.6f lag=%.6f rho=%.9f cents=%.6f voiced=%d splice=%d\n",
                         i, dev, y[i], x[i - (size_t) d], f.lag, f.ratio, f.appliedCents, (int) f.voiced, (int) f.splice);
            ++printed;
        }
    }
    return 0;
}

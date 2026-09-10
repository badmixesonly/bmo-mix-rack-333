// Scratch diagnostic, not a test: prints the frames a signal gets wrong.
#include "modules/tune/dsp/Detector.h"
#include "modules/tune/dsp/Pitch.h"
#include "tools/common/Signals.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace bmo::tune;
namespace sig = bmo::tune::signals;

int main (int argc, char** argv)
{
    const double hz = argc > 1 ? std::atof (argv[1]) : 146.83;
    const char* kind = argc > 2 ? argv[2] : "sine";
    const double fs = argc > 3 ? std::atof (argv[3]) : 48000.0;
    const double secs = 0.6;

    auto c = sig::steady (hz, secs, fs);
    if (! std::strcmp (kind, "glide")) c = sig::glide (110.0, 880.0, 3.0, fs);
    if (! std::strcmp (kind, "vib")) c = sig::vibrato (330.0, 100.0, 5.5, 2.0, fs);
    if (! std::strncmp (kind, "onset", 5)) c = sig::concat ({ sig::silence (0.1, fs), sig::steady (hz, 0.4, fs) });

    std::vector<float> x;
    if (! std::strcmp (kind, "sine") || ! std::strcmp (kind, "onsetsine")) x = sig::sine (c, fs).samples;
    else if (! std::strcmp (kind, "saw")) x = sig::sawtooth (c, fs).samples;
    else x = sig::voice (c, fs).samples;

    Detector d;
    d.prepare (fs, {});
    int printed = 0;

    for (size_t i = 0; i < x.size(); ++i)
    {
        d.push (x[i]);
        if (! d.evaluatedThisSample() || c[i] <= 0.0) continue;
        const auto& e = d.estimate();
        const auto cents = e.hz > 0 ? pitch::centsBetween (e.hz, c[i]) : 9999.0;
        if ((std::abs (cents) > 50.0 || ! e.voiced) && printed < 60)
        {
            std::printf ("t=%8.2f ms truth=%8.2f est=%8.2f cents=%9.1f clar=%.3f voiced=%d rms=%.4f cand=%8.2f Hz\n",
                         1000.0 * i / fs, c[i], e.hz, cents, e.clarity, (int) e.voiced, e.rms,
                         e.candidate > 0 ? fs / e.candidate : 0.0);
            ++printed;
        }
    }
    return 0;
}

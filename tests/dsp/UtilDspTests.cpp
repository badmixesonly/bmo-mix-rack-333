/*
    BMO Util's DSP, JUCE-free. Each control is checked on its own against
    the arithmetic it claims: a gain of -6 dB halves, pan is a balance law,
    width 0 is mono, width 200 doubles the side, polarity flips, mono sums.
*/

#include "modules/util/dsp/UtilDsp.h"
#include <iostream>
#include <vector>

using namespace bmo::util;

namespace
{
    int failures = 0;

    void check (bool ok, const char* what)
    {
        if (! ok) { std::cerr << "FAIL: " << what << '\n'; ++failures; }
    }

    bool near (float a, float b, float tol = 1.0e-3f) { return std::abs (a - b) <= tol; }

    struct Run
    {
        std::vector<float> l, r;
    };

    /** Runs a constant stereo pair through the DSP long enough for the
        smoothers to settle, and returns the last sample. */
    Run run (float gainDb, float pan, float width, bool phL, bool phR, bool mono,
             float inL, float inR)
    {
        UtilDsp dsp;
        const float v[Index::count] { gainDb, pan, width, phL ? 1.0f : 0.0f, phR ? 1.0f : 0.0f, mono ? 1.0f : 0.0f };
        dsp.setParams (v, Index::count);
        dsp.prepare (48000.0, 512, 2);

        constexpr int n = 48000;
        std::vector<float> l ((size_t) n, inL), r ((size_t) n, inR);
        float* ch[2] { l.data(), r.data() };
        dsp.process (ch, 2, n);

        return { l, r };
    }
}

int main()
{
    // Defaults are a wire.
    {
        auto out = run (0.0f, 0.0f, 100.0f, false, false, false, 0.5f, -0.25f);
        check (near (out.l.back(), 0.5f) && near (out.r.back(), -0.25f), "defaults pass audio unchanged");
    }

    // Gain.
    {
        auto out = run (-6.0206f, 0.0f, 100.0f, false, false, false, 1.0f, 1.0f);
        check (near (out.l.back(), 0.5f, 2.0e-3f), "-6 dB halves");
    }

    // Pan: balance law, centre is unity and each side only attenuates the other.
    {
        auto out = run (0.0f, -100.0f, 100.0f, false, false, false, 1.0f, 1.0f);
        check (near (out.l.back(), 1.0f) && near (out.r.back(), 0.0f), "hard left silences the right");

        out = run (0.0f, 50.0f, 100.0f, false, false, false, 1.0f, 1.0f);
        check (near (out.l.back(), 0.5f) && near (out.r.back(), 1.0f), "half right takes half off the left");
    }

    // Width.
    {
        auto out = run (0.0f, 0.0f, 0.0f, false, false, false, 1.0f, 0.0f);
        check (near (out.l.back(), 0.5f) && near (out.r.back(), 0.5f), "width 0 is mono");

        out = run (0.0f, 0.0f, 200.0f, false, false, false, 1.0f, 0.0f);
        // mid 0.5, side 0.5 * 2 = 1.0 -> L 1.5, R -0.5
        check (near (out.l.back(), 1.5f) && near (out.r.back(), -0.5f), "width 200 doubles the side");
    }

    // Polarity.
    {
        auto out = run (0.0f, 0.0f, 100.0f, true, false, false, 0.5f, 0.5f);
        check (near (out.l.back(), -0.5f) && near (out.r.back(), 0.5f), "phase L flips only the left");

        out = run (0.0f, 0.0f, 100.0f, true, true, false, 0.5f, 0.5f);
        check (near (out.l.back(), -0.5f) && near (out.r.back(), -0.5f), "both flip both");
    }

    // Mono.
    {
        auto out = run (0.0f, 0.0f, 100.0f, false, false, true, 1.0f, 0.0f);
        check (near (out.l.back(), 0.5f) && near (out.r.back(), 0.5f), "mono sums to (L+R)/2 on both");
    }

    // A parameter change ramps rather than steps.
    {
        UtilDsp dsp;
        float v[Index::count] { 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f };
        dsp.setParams (v, Index::count);
        dsp.prepare (48000.0, 512, 2);

        v[Index::gain] = -24.0f;
        dsp.setParams (v, Index::count);

        std::vector<float> l (64, 1.0f), r (64, 1.0f);
        float* ch[2] { l.data(), r.data() };
        dsp.process (ch, 2, 64);

        check (l[0] > 0.9f && l[63] < l[0], "gain changes are smoothed");
    }

    // Mono input does not crash and gets gain and polarity.
    {
        UtilDsp dsp;
        const float v[Index::count] { -6.0206f, 0.0f, 100.0f, 1.0f, 0.0f, 0.0f };
        dsp.setParams (v, Index::count);
        dsp.prepare (48000.0, 512, 1);

        std::vector<float> l (48000, 1.0f);
        float* ch[1] { l.data() };
        dsp.process (ch, 1, 48000);
        check (near (l.back(), -0.5f, 2.0e-3f), "mono input gets gain and polarity");
    }

    check (UtilDsp().latencyForParams (nullptr, 0) == 0, "no latency");

    if (failures == 0)
        std::cout << "All BMO Util DSP tests passed.\n";

    return failures == 0 ? 0 : 1;
}

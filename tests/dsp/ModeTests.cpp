/*
    The rule that the panel hides HYBRID-only controls on CLASSIC, held to the
    DSP so it cannot drift.

    `isHybridOnly()` in modules/tune/params.h is the one list the panel reads.
    This file checks it against what the engines actually do:

      - every parameter on the list is exactly inert on CLASSIC: moving it to
        an extreme leaves CLASSIC's output bit-identical. If one ever starts
        to matter on CLASSIC, hiding it there hides something that works.
      - every parameter on the list is audible on HYBRID. If one stops
        mattering, it is taking panel space for nothing.
      - the list is exactly the three it is today, so adding a fourth is a
        decision recorded here rather than a side effect.
*/

#include "modules/tune/dsp/TuneCore.h"
#include "tests/TestUtil.h"
#include "tools/common/Signals.h"

#include <string>

using namespace bmo::tune;
using namespace bmo::tune::test;
namespace sig = bmo::tune::signals;

namespace
{
    constexpr double fs = 48000.0;

    std::vector<float> render (const std::vector<float>& in, const TuneParams& p)
    {
        TuneCore core;
        core.setParams (p);
        core.prepare (fs, 512);
        auto out = in;
        for (size_t at = 0; at < out.size(); at += 256)
            core.process (out.data() + at, (int) std::min<size_t> (256, out.size() - at));
        return out;
    }

    double worstDifference (const std::vector<float>& a, const std::vector<float>& b)
    {
        double worst = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            worst = std::max (worst, (double) std::abs (a[i] - b[i]));
        return worst;
    }

    /** A voice that needs correcting and changes note, so Glide has a note
        change to act on and the formant controls have a correction to act
        with: 20 cents sharp of A3, then a semitone up, 20 cents flat. */
    std::vector<float> material()
    {
        const auto c = sig::concat ({ sig::steady (220.0 * std::exp2 (20.0 / 1200.0), 0.4, fs),
                                      sig::steady (233.08 * std::exp2 (-20.0 / 1200.0), 0.4, fs) });
        return sig::voice (c, fs).samples;
    }

    /** Sets one HYBRID-only parameter to a value far from its default. */
    void moveFar (TuneParams& p, int index)
    {
        switch (index)
        {
            case Index::glide:        p.glideMs = 150.0; break;
            case Index::formant:      p.formant = false; break;
            case Index::formantShift: p.formantShiftCents = 400.0; break;
            default: break;
        }
    }
}

int main()
{
    //== The list is the list ===================================================
    {
        std::string listed;
        int count = 0;
        for (int i = 0; i < Index::count; ++i)
            if (isHybridOnly (i)) { listed += std::string (specs()[(size_t) i].id) + " "; ++count; }

        report ("HYBRID-only parameters", count);
        check (listed == "glide formant formant_shift ", "the HYBRID-only list is exactly glide, formant, formant_shift -- got: " + listed);
    }

    const auto x = material();

    //== Inert on CLASSIC, each one and all together ============================
    {
        TuneParams classic;                    // CLASSIC, chromatic, retune 0
        const auto reference = render (x, classic);

        auto all = classic;
        for (int i = 0; i < Index::count; ++i)
        {
            if (! isHybridOnly (i))
                continue;

            auto one = classic;
            moveFar (one, i);
            moveFar (all, i);
            const auto d = worstDifference (render (x, one), reference);
            check (d == 0.0, std::string (specs()[(size_t) i].id) + " is exactly inert on CLASSIC, so hiding it there hides nothing");
        }

        check (worstDifference (render (x, all), reference) == 0.0, "all HYBRID-only parameters moved at once leave CLASSIC bit-identical");
    }

    //== Audible on HYBRID ======================================================
    {
        TuneParams hybrid;
        hybrid.engine = Engine::hybrid;
        const auto reference = render (x, hybrid);

        for (int i = 0; i < Index::count; ++i)
        {
            if (! isHybridOnly (i))
                continue;

            auto one = hybrid;
            moveFar (one, i);
            const auto d = worstDifference (render (x, one), reference);
            report (std::string ("HYBRID, ") + specs()[(size_t) i].id + " moved: worst sample difference", d);
            check (d > 1.0e-3, std::string (specs()[(size_t) i].id) + " is audible on HYBRID, so showing it there shows something");
        }
    }

    return finish ("mode");
}

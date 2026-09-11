/*
    Voices the synthetic corpus did not have, rebuilt from what went wrong on
    real ones. Each section names the take and the moment it came from.

    1. A weak fundamental (Failure, 2026-09-11 shoot-out; Frosty ranked BMO
       last on all three Failure groups: "glitchy uneven correction, audible
       pops and clicks", "hunting for pitch"). At 1.06 s the singer holds a
       D4 about 37 cents sharp, 300 Hz, on an /a/ whose first formant sits on
       the octave, and the fundamental is well under the second harmonic.
       The detector read 565-644 Hz, swinging with every evaluation, and the
       note decision flipped between D5 and E5 427 times over the take.
       Measured on the real take (field-audio, not committed): 8.5 % of
       voiced frames an octave up, 2.0 % a twelfth up, and the estimate
       spread 233 cents inside 10 ms on average -- against 0.4 %, 0.1 % and
       59 cents on Fuji, where BMO ranked better.

    Truth here is the synthetic contour; the output is measured with
    tools/common/Analysis.h, never with the plugin's own detector.
*/

#include "modules/tune/dsp/Detector.h"
#include "modules/tune/dsp/TuneCore.h"
#include "tests/TestUtil.h"
#include "tools/common/Analysis.h"
#include "tools/common/Signals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace bmo::tune;
using namespace bmo::tune::test;
namespace sig = bmo::tune::signals;
namespace an = bmo::tune::analysis;

namespace
{
    constexpr double fs = 48000.0;

    sig::VoiceSettings weakFundamental (double db, std::uint64_t seed)
    {
        sig::VoiceSettings v;
        v.fundamentalDb = db;
        v.jitter = 0.003;    // a sung note is never a machine's
        v.shimmer = 0.03;
        v.seed = seed;
        return v;
    }

    struct DetectorRead
    {
        double right = 0.0, octaveUp = 0.0, twelfthUp = 0.0, octaveDown = 0.0;   // fractions of voiced evaluations
        double spreadCents = 0.0;                               // mean max/min inside 10 ms
        int voiced = 0;
    };

    /** The detector alone on a steady note, from 0.15 s (after lock) on. */
    DetectorRead readDetector (const std::vector<float>& x, double truthHz)
    {
        Detector d;
        d.prepare (fs, {});
        DetectorRead r;
        std::vector<double> window;
        long long windowStart = -1;
        double spreadSum = 0.0;
        int spreadN = 0, right = 0, up8 = 0, up12 = 0, down8 = 0;

        for (size_t i = 0; i < x.size(); ++i)
        {
            d.push (x[i]);
            if (! d.evaluatedThisSample() || i < (size_t) (0.15 * fs))
                continue;

            const auto& e = d.estimate();
            if (! e.voiced || e.hz <= 0.0)
                continue;

            ++r.voiced;
            const auto c = an::cents (e.hz, truthHz);
            if (std::abs (c) < 50.0) ++right;
            else if (std::abs (c - 1200.0) < 80.0) ++up8;
            else if (std::abs (c - 1902.0) < 80.0) ++up12;
            else if (std::abs (c + 1200.0) < 80.0) ++down8;

            const auto block = (long long) (i / (size_t) (0.010 * fs));
            if (block != windowStart)
            {
                if (window.size() > 2)
                {
                    const auto mm = std::minmax_element (window.begin(), window.end());
                    spreadSum += 1200.0 * std::log2 (*mm.second / *mm.first);
                    ++spreadN;
                }
                window.clear();
                windowStart = block;
            }
            window.push_back (e.hz);
        }

        if (r.voiced > 0)
        {
            r.right = (double) right / r.voiced;
            r.octaveUp = (double) up8 / r.voiced;
            r.twelfthUp = (double) up12 / r.voiced;
            r.octaveDown = (double) down8 / r.voiced;
        }
        r.spreadCents = spreadN ? spreadSum / spreadN : 0.0;
        return r;
    }

    std::vector<float> render (const std::vector<float>& in, const TuneParams& p)
    {
        TuneCore core;
        core.setParams (p);
        core.prepare (fs, 128);
        auto y = in;
        for (size_t at = 0; at < y.size(); at += 128)
            core.process (y.data() + at, (int) std::min<size_t> (128, y.size() - at));
        return y;
    }

    std::string label (const char* what, double a, double b)
    {
        char buf[160];
        std::snprintf (buf, sizeof buf, what, a, b);
        return buf;
    }
}

int main()
{
    //== 1. A weak fundamental: the detector ===================================
    std::printf ("weak fundamental: the detector's read of a held note\n");
    for (const auto db : { 0.0, -20.0, -26.0 })
    {
        for (const auto hz : { 150.0, 200.0, 250.0, 300.0, 350.0 })
        {
            const auto x = sig::voice (sig::steady (hz, 0.8, fs), fs, weakFundamental (db, 7000u + (unsigned) hz)).samples;
            const auto r = readDetector (x, hz);

            const auto name = label ("%.0f Hz, fundamental %+.0f dB", hz, db);
            report (name + ": on the note", 100.0 * r.right, "%");
            report (name + ": an octave up", 100.0 * r.octaveUp, "%");
            report (name + ": an octave down", 100.0 * r.octaveDown, "%");
            report (name + ": spread inside 10 ms", r.spreadCents, "c");

            check (r.voiced > 0 && r.right >= 0.99, name + ": the detector reads the fundamental on 99 % of voiced evaluations");
            check (r.spreadCents <= 20.0, name + ": and its estimate holds within 20 cents inside 10 ms");
        }
    }

    //== 1. A weak fundamental: through the whole plugin ======================
    // The Failure moment itself: D4 37 cents sharp, D major, retune 0. Held,
    // the output should sit on D4 -- measured 40 ms at a time every 10 ms by
    // the ruler, kept to the voice's own octave so it cannot be fooled the
    // way the plugin was.
    std::printf ("weak fundamental: a held D4, 37 cents sharp, D major, 0.0 ms\n");
    for (const auto db : { 0.0, -20.0, -26.0 })
    {
        const auto sung = 293.6648 * std::exp2 (37.0 / 1200.0);
        const auto x = sig::voice (sig::steady (sung, 1.2, fs), fs, weakFundamental (db, 9100u)).samples;

        TuneParams p;
        p.key = 2;
        p.scale = ScaleType::major;
        const auto y = render (x, p);

        double worst = 0.0;
        int frames = 0, off = 0;
        for (size_t a = (size_t) (0.25 * fs); a + (size_t) (0.04 * fs) < y.size() - (size_t) (0.1 * fs); a += (size_t) (0.01 * fs))
        {
            const auto hz = an::measureHz (y, a, (size_t) (0.04 * fs), fs, 200.0, 420.0);
            const auto c = hz > 0.0 ? an::cents (hz, 293.6648) : 1.0e3;
            worst = std::max (worst, std::abs (c));
            off += std::abs (c) > 5.0 ? 1 : 0;
            ++frames;
        }

        const auto name = label ("held D4, fundamental %+.0f dB", db, 0.0);
        report (name + ": worst output frame off D4", worst, "c");
        report (name + ": output frames more than 5 c off", (double) off);
        check (worst <= 5.0, name + ": a held note comes out on D4, every 10 ms frame within 5 cents");
    }

    return finish ("voice");
}

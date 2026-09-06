/*
    Tests for BMO Opto's DSP core. No JUCE, no host: DspCore takes plain
    buffers, so the four claims the module is built on -- it leaves a quiet
    signal alone, CRUSH makes it grab harder, LEVEL adds exactly what it
    says, and the release gets slower after a heavier hit -- are checked in a
    couple of seconds on a bare container.
*/

#include "modules/opto/dsp/DspCore.h"
#include "modules/opto/dsp/OptoDsp.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace bmo::opto;

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;

int failures = 0, checks = 0;

void check (bool condition, const std::string& what)
{
    ++checks;

    if (! condition)
    {
        std::printf ("FAIL  %s\n", what.c_str());
        ++failures;
    }
}

void checkNear (double value, double expected, double tolerance, const std::string& what)
{
    ++checks;

    if (! (std::abs (value - expected) <= tolerance))
    {
        std::printf ("FAIL  %s: %.6f, expected %.6f +/- %.6f\n",
                     what.c_str(), value, expected, tolerance);
        ++failures;
    }
}

std::vector<float> sine (double hz, double seconds, double amplitude)
{
    const auto n = (size_t) (seconds * kSampleRate);
    std::vector<float> out (n);

    for (size_t i = 0; i < n; ++i)
        out[i] = (float) (amplitude * std::sin (2.0 * kPi * hz * (double) i / kSampleRate));

    return out;
}

std::vector<float> render (const std::vector<float>& input, DspCore::Params params)
{
    DspCore core;
    core.prepare (kSampleRate, 512, 1);
    core.setParams (params);

    auto out = input;

    for (size_t at = 0; at < out.size(); at += 512)
    {
        auto* p = out.data() + at;
        core.process (&p, 1, (int) std::min<size_t> (512, out.size() - at));
    }

    return out;
}

//==============================================================================
/** Well under Crush 0's threshold (-8 dB, 16 dB knee -- nothing engages below
    -16 dB), a quiet tone should come back essentially as it went in. */
void testQuietSignalIsLeftAlone()
{
    DspCore::Params p;
    p.crushPercent = 0.0f;
    p.levelDb = 0.0f;

    const auto dry = sine (1000.0, 1.0, 0.05);   // ~ -26 dBFS
    const auto wet = render (dry, p);

    double worst = 0.0;
    for (size_t i = 4000; i < dry.size(); ++i)
        worst = std::max (worst, (double) std::abs (dry[i] - wet[i]));

    check (worst < 0.01, "a quiet tone under threshold passes essentially unchanged at Crush 0");
}

/** CRUSH lowers the threshold and raises the ratio together, so a loud tone
    should be reduced more, or at least no less, as CRUSH rises. */
void testReductionRisesWithCrush()
{
    const auto dry = sine (1000.0, 1.0, 0.5);   // -6 dBFS: loud enough to engage even at Crush 0
    double previous = -1.0;

    for (float crush : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
    {
        DspCore core;
        DspCore::Params p;
        p.crushPercent = crush;
        core.prepare (kSampleRate, 512, 1);
        core.setParams (p);

        auto out = dry;
        for (size_t at = 0; at < out.size(); at += 512)
        {
            auto* pp = out.data() + at;
            core.process (&pp, 1, (int) std::min<size_t> (512, out.size() - at));
        }

        const auto gr = core.currentGainReductionDb();
        check (gr >= previous - 1.0e-6, "reduction at Crush " + std::to_string ((int) crush) + " is not less than the setting below it");
        previous = gr;
    }

    check (previous > 3.0, "a loud tone is meaningfully reduced at Crush 100");
}

/** With nothing engaging (Crush 0, a quiet source), LEVEL is the only thing
    touching the signal, so it must add exactly what it says. */
void testMakeupGainIsExact()
{
    const auto dry = sine (1000.0, 1.0, 0.05);

    DspCore::Params p;
    p.crushPercent = 0.0f;
    p.levelDb = 6.0f;

    const auto wet = render (dry, p);

    double drySum = 0.0, wetSum = 0.0;
    for (size_t i = 4000; i < dry.size(); ++i)
    {
        drySum += (double) dry[i] * dry[i];
        wetSum += (double) wet[i] * wet[i];
    }

    const auto ratioDb = 10.0 * std::log10 (wetSum / drySum);
    checkNear (ratioDb, 6.0, 0.3, "Level +6 dB adds ~6 dB when nothing is being reduced");
}

/** The whole point of the program-dependent release: recovering from a long,
    heavy hit takes longer than recovering from a short one. */
void testReleaseIsProgramDependent()
{
    const auto timeToRecoverAfter = [] (double loudSeconds) -> double
    {
        DspCore core;
        DspCore::Params p;
        p.crushPercent = 80.0f;
        core.prepare (kSampleRate, 512, 1);
        core.setParams (p);

        auto signal = sine (200.0, loudSeconds, 0.9);
        const std::vector<float> silence ((size_t) (kSampleRate * 3.0), 0.0f);
        signal.insert (signal.end(), silence.begin(), silence.end());

        const auto loudSamples = (size_t) (loudSeconds * kSampleRate);
        size_t recoveredAt = signal.size();

        for (size_t at = 0; at < signal.size(); at += 512)
        {
            auto* pp = signal.data() + at;
            const auto n = (int) std::min<size_t> (512, signal.size() - at);
            core.process (&pp, 1, n);

            if (at >= loudSamples && core.currentGainReductionDb() < 0.5f)
            {
                recoveredAt = at;
                break;
            }
        }

        return (double) (recoveredAt - loudSamples) / kSampleRate;
    };

    const auto shortRecovery = timeToRecoverAfter (0.2);
    const auto longRecovery  = timeToRecoverAfter (2.5);

    check (longRecovery > shortRecovery,
           "recovery after a long, heavy hit (" + std::to_string (longRecovery)
             + "s) takes longer than after a short one (" + std::to_string (shortRecovery) + "s)");
}

/** Nothing here may produce a NaN, an infinity, or a runaway. */
void testStability()
{
    std::vector<float> nasty;
    for (int i = 0; i < 48000; ++i)
        nasty.push_back ((float) ((i / 64) % 2 == 0 ? 3.0 : -3.0));   // past full scale

    DspCore::Params p;
    p.crushPercent = 100.0f;
    p.levelDb = 24.0f;

    for (auto v : render (nasty, p))
        check (std::isfinite (v) && std::abs (v) < 60.0f, "the output stays finite and bounded under an extreme input");
}

/** No lookahead, no oversampling: this module must never cost the host a
    sample of plugin-delay-compensation. */
void testLatencyIsAlwaysZero()
{
    OptoDsp dsp;
    dsp.prepare (kSampleRate, 512, 2);

    const float values[] { 0.0f, 0.0f };
    check (dsp.latencyForParams (values, 2) == 0, "Crush 0 reports zero latency");

    const float loud[] { 100.0f, 24.0f };
    check (dsp.latencyForParams (loud, 2) == 0, "Crush 100 also reports zero latency");
}

} // namespace

//==============================================================================
int main()
{
    testQuietSignalIsLeftAlone();
    testReductionRisesWithCrush();
    testMakeupGainIsExact();
    testReleaseIsProgramDependent();
    testStability();
    testLatencyIsAlwaysZero();

    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

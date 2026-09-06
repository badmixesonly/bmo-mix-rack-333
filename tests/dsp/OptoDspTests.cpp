/*
    Tests for BMO Opto's DSP core. No JUCE, no host: DspCore takes plain
    buffers, so the claims the module is built on are checked in a couple of
    seconds on a bare container: it leaves a quiet signal alone, CRUSH makes
    it grab harder, LEVEL adds exactly what it says, the release gets slower
    after a heavier/longer hit (and does so differently per mode), Stressed
    mode's ratio genuinely differs from Tele's, stereo Link actually shares
    gain reduction, and Color's Tele-mode lock actually holds.
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

/** Runs an L/R pair through a fresh core and returns { outL, outR }. */
std::pair<std::vector<float>, std::vector<float>> renderStereo (
    const std::vector<float>& l, const std::vector<float>& r, DspCore::Params params)
{
    DspCore core;
    core.prepare (kSampleRate, 512, 2);
    core.setParams (params);

    auto outL = l, outR = r;

    for (size_t at = 0; at < outL.size(); at += 512)
    {
        const auto n = (int) std::min<size_t> (512, outL.size() - at);
        float* channels[2] { outL.data() + at, outR.data() + at };
        core.process (channels, 2, n);
    }

    return { outL, outR };
}

double rms (const std::vector<float>& v, size_t from = 0)
{
    double sum = 0.0;
    for (size_t i = from; i < v.size(); ++i) sum += (double) v[i] * v[i];
    return std::sqrt (sum / (double) (v.size() - from));
}

//==============================================================================
/** Well under Tele's Crush 0 threshold (-8 dB, 16 dB knee -- nothing engages
    below -16 dB), a quiet tone should come back essentially as it went in. */
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

/** CRUSH only moves threshold now (ratio/knee are fixed per mode), so a loud
    tone should still be reduced more, or at least no less, as CRUSH rises. */
void testReductionRisesWithCrush()
{
    const auto dry = sine (1000.0, 1.0, 0.5);   // -6 dBFS: loud enough to engage even at Crush 0
    double previous = -1.0;

    for (float crush : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
    {
        DspCore::Params p;
        p.crushPercent = crush;

        DspCore core;
        core.prepare (kSampleRate, 512, 1);
        core.setParams (p);

        auto out = dry;
        for (size_t at = 0; at < out.size(); at += 512)
        {
            auto* pp = out.data() + at;
            core.process (&pp, 1, (int) std::min<size_t> (512, out.size() - at));
        }

        const auto reduction = core.currentGainReductionDb();
        check (reduction >= previous - 1.0e-6, "reduction at Crush " + std::to_string ((int) crush) + " is not less than the setting below it");
        previous = reduction;
    }

    check (previous > 3.0, "a loud tone is meaningfully reduced at Crush 100");
}

/** With nothing engaging (Crush 0, a quiet source, Color off), LEVEL is the
    only thing touching the signal, so it must add exactly what it says. */
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

/** How much reduction (dB) is still showing `silenceSeconds` after a hit of
    `loudSeconds` ends. Measuring "how much is left at a fixed checkpoint"
    rather than racing to a full-recovery threshold, because the release
    tail is long enough (into the tens of seconds at the ceiling) that a
    race could need an impractically long silence buffer to ever finish --
    a fixed checkpoint mid-decay is exactly as good a test of "did the long
    hit release slower" and doesn't depend on getting that buffer length
    right. */
float reductionAfter (double loudSeconds, double silenceSeconds, float crushPercent, Mode mode)
{
    DspCore core;
    DspCore::Params p;
    p.crushPercent = crushPercent;
    p.mode = mode;
    core.prepare (kSampleRate, 512, 1);
    core.setParams (p);

    auto signal = sine (200.0, loudSeconds, 0.9);
    const std::vector<float> silence ((size_t) (kSampleRate * silenceSeconds), 0.0f);
    signal.insert (signal.end(), silence.begin(), silence.end());

    auto reduction = 0.0f;

    for (size_t at = 0; at < signal.size(); at += 512)
    {
        auto* pp = signal.data() + at;
        const auto n = (int) std::min<size_t> (512, signal.size() - at);
        core.process (&pp, 1, n);
        reduction = core.currentGainReductionDb();
    }

    return reduction;
}

/** Reduction right when a hit ends, and again `silenceSeconds` later, from
    the same run -- so a residual can be read as a *fraction* of where it
    started, rather than an absolute dB figure. */
std::pair<float, float> reductionAtEndAndAfter (double loudSeconds, double silenceSeconds, float crushPercent, Mode mode)
{
    DspCore core;
    DspCore::Params p;
    p.crushPercent = crushPercent;
    p.mode = mode;
    core.prepare (kSampleRate, 512, 1);
    core.setParams (p);

    auto signal = sine (200.0, loudSeconds, 0.9);
    const std::vector<float> silence ((size_t) (kSampleRate * silenceSeconds), 0.0f);
    signal.insert (signal.end(), silence.begin(), silence.end());

    const auto loudSamples = (size_t) (loudSeconds * kSampleRate);
    auto atEnd = 0.0f, atCheckpoint = 0.0f;

    for (size_t at = 0; at < signal.size(); at += 512)
    {
        auto* pp = signal.data() + at;
        const auto n = (int) std::min<size_t> (512, signal.size() - at);
        core.process (&pp, 1, n);
        const auto reduction = core.currentGainReductionDb();

        if (at < loudSamples && at + (size_t) n >= loudSamples)
            atEnd = reduction;

        atCheckpoint = reduction;
    }

    return { atEnd, atCheckpoint };
}

/** The whole point of the dosage-dependent release: a long, heavy hit still
    shows more reduction a fixed few seconds later than a short one does, in
    both modes -- though not by the same margin, since the two modes don't
    use the same depth of model on purpose (see Detector.h). Tele's release
    tau is itself a continuous, growing function of accumulated drive, so a
    2.5s hit and a 0.2s hit land on genuinely different taus (a wide spread
    -- roughly 1.9s vs 8.9s by hand -- so the gap is easily seconds of
    residual reduction). Stressed's simpler single charge-blend only shifts
    *how much* of one fixed floor-to-ceiling range it reaches, a smaller
    effect (tenths of a dB at this checkpoint by hand) -- correctly smaller
    given it's the intentionally simpler of the two models, not a bug. */
void testReleaseIsProgramDependent()
{
    const auto afterShortTele = reductionAfter (0.2, 3.0, 80.0f, Mode::La2a);
    const auto afterLongTele  = reductionAfter (2.5, 3.0, 80.0f, Mode::La2a);

    check (afterLongTele > afterShortTele + 0.5f,
           "Tele: 3s after the hit ends, a long one (" + std::to_string (afterLongTele)
             + " dB left) still shows more reduction than a short one (" + std::to_string (afterShortTele) + " dB left)");

    const auto afterShortStressed = reductionAfter (0.2, 3.0, 80.0f, Mode::Distressor);
    const auto afterLongStressed  = reductionAfter (2.5, 3.0, 80.0f, Mode::Distressor);

    check (afterLongStressed > afterShortStressed + 0.05f,
           "Stressed: 3s after the hit ends, a long one (" + std::to_string (afterLongStressed)
             + " dB left) still shows more reduction than a short one (" + std::to_string (afterShortStressed) + " dB left)");
}

/** Stressed's release ceiling (~20 s) is meant to reach further than Tele's
    (~15 s) for the same very long, heavy hit -- that's the one numeric
    difference in an otherwise similarly-shaped release model. Checked as a
    *fraction* of each mode's own starting reduction, not an absolute dB
    figure: Stressed's fixed 10:1 ratio starts from a much deeper reduction
    than Tele's fixed 3:1 (already covered by testDistressorRatioExceedsLa2a),
    so comparing raw dB left over would mostly just re-measure that ratio
    difference rather than the release timing this test is actually about. */
void testDistressorReleaseCeilingExceedsLa2a()
{
    const auto [teleEnd, teleAfter]         = reductionAtEndAndAfter (8.0, 10.0, 90.0f, Mode::La2a);
    const auto [stressedEnd, stressedAfter] = reductionAtEndAndAfter (8.0, 10.0, 90.0f, Mode::Distressor);

    const auto teleFraction     = teleAfter / teleEnd;
    const auto stressedFraction = stressedAfter / stressedEnd;

    check (stressedFraction > teleFraction + 0.02,
           "Stressed retains a larger fraction of its starting reduction (" + std::to_string (stressedFraction)
             + ") than Tele does (" + std::to_string (teleFraction) + ") 10s after the same long, heavy hit");
}

/** Stressed's fixed 10:1 ratio should catch harder than Tele's fixed 3:1 at
    the same Crush setting and input level -- the one thing CRUSH does not
    equalize between modes, on purpose (see curveForLa2a/curveForDistressor). */
void testDistressorRatioExceedsLa2a()
{
    const auto dry = sine (1000.0, 1.0, 0.5);

    const auto reductionFor = [&] (Mode mode) -> float
    {
        DspCore core;
        DspCore::Params p;
        p.crushPercent = 50.0f;
        p.mode = mode;
        core.prepare (kSampleRate, 512, 1);
        core.setParams (p);

        auto out = dry;
        for (size_t at = 0; at < out.size(); at += 512)
        {
            auto* pp = out.data() + at;
            core.process (&pp, 1, (int) std::min<size_t> (512, out.size() - at));
        }

        return core.currentGainReductionDb();
    };

    const auto tele = reductionFor (Mode::La2a);
    const auto stressed = reductionFor (Mode::Distressor);

    check (stressed > tele, "Stressed's fixed 10:1 ratio reduces more than Tele's fixed 3:1 at the same Crush");
}

/** Unlinked, two channels at genuinely different levels should compress
    independently, so a fixed level ratio between them does not survive.
    Linked, one shared cell decides the gain for both, so it does -- exactly,
    since it is literally the same multiply applied to both channels. */
void testStereoLink()
{
    const auto l = sine (1000.0, 1.5, 0.8);
    std::vector<float> r (l.size());
    for (size_t i = 0; i < l.size(); ++i) r[i] = l[i] * 0.25f;   // quieter, but proportional

    for (bool linked : { false, true })
    {
        DspCore::Params p;
        p.crushPercent = 70.0f;
        p.link = linked;

        const auto [outL, outR] = renderStereo (l, r, p);
        const auto ratio = rms (outR, l.size() / 2) / rms (outL, l.size() / 2);

        if (linked)
            checkNear (ratio, 0.25, 0.01, "linked: R stays proportional to L (same shared gain on both channels)");
        else
            check (std::abs (ratio - 0.25) > 0.02, "unlinked: R/L ratio moves away from the input's 0.25 once each channel compresses on its own");
    }
}

/** In Stressed mode, Color is a real toggle: on adds harmonic content, off
    leaves the signal alone. (Tele has no off state for Color to compare
    against -- see testTeleColorIsLocked instead.) Checked as "the waveform
    differs from a pure linear copy," not a specific harmonic number, since
    that's what the toggle is actually claiming. */
void testColorTogglesHarmonics()
{
    const auto dry = sine (300.0, 0.5, 0.6);

    DspCore::Params off;
    off.crushPercent = 0.0f;   // minimal cell action; identical in both runs either way
    off.mode = Mode::Distressor;
    off.color = false;

    DspCore::Params on = off;
    on.color = true;

    const auto wetOff = render (dry, off);
    const auto wetOn  = render (dry, on);

    double diff = 0.0;
    for (size_t i = 4000; i < dry.size(); ++i)
        diff = std::max (diff, (double) std::abs (wetOff[i] - wetOn[i]));

    check (diff > 0.01, "Stressed: Color on measurably differs from Color off");
}

/** Tele always runs its drive stage, regardless of what the Color parameter
    says -- the panel is expected to hide/disable the switch there, but the
    DSP has to hold the lock even if something else sets the parameter. */
void testTeleColorIsLocked()
{
    const auto dry = sine (300.0, 0.5, 0.6);

    DspCore::Params colorOff;
    colorOff.crushPercent = 0.0f;   // minimal cell action; identical in every run below either way
    colorOff.mode = Mode::La2a;
    colorOff.color = false;

    DspCore::Params colorOn = colorOff;
    colorOn.color = true;

    const auto wetColorOff = render (dry, colorOff);
    const auto wetColorOn  = render (dry, colorOn);

    check (wetColorOff == wetColorOn, "Tele: output is identical whether Color's parameter is off or on -- the lock ignores it");

    // And the lock isn't hiding a no-op: La2aDrive is a genuine nonlinearity
    // (checked directly, not by comparing against a different mode's
    // output, which would also differ for cell/topology reasons that have
    // nothing to do with Color) -- a fixed multiply would give the same
    // input/output gain at any level; a saturator's gain changes with it.
    La2aDrive lowShaper, highShaper;
    const auto gainLow  = lowShaper.process (0.1f) / 0.1f;
    const auto gainHigh = highShaper.process (0.9f) / 0.9f;

    check (std::abs (gainHigh - gainLow) > 0.02f,
           "La2aDrive's gain at 0.9 (" + std::to_string (gainHigh) + ") differs from its gain at 0.1 ("
             + std::to_string (gainLow) + ") -- it's a nonlinearity, not a fixed multiply");
}

/** Nothing here may produce a NaN, an infinity, or a runaway, in either
    mode, linked or not, Color on or off. */
void testStability()
{
    std::vector<float> nasty;
    for (int i = 0; i < 48000; ++i)
        nasty.push_back ((float) ((i / 64) % 2 == 0 ? 3.0 : -3.0));   // past full scale

    for (auto mode : { Mode::La2a, Mode::Distressor })
    {
        for (bool link : { false, true })
        {
            DspCore::Params p;
            p.crushPercent = 100.0f;
            p.levelDb = 24.0f;
            p.mode = mode;
            p.link = link;
            p.color = true;

            DspCore core;
            core.prepare (kSampleRate, 512, 2);
            core.setParams (p);

            auto l = nasty, r = nasty;

            for (size_t at = 0; at < l.size(); at += 512)
            {
                const auto n = (int) std::min<size_t> (512, l.size() - at);
                float* channels[2] { l.data() + at, r.data() + at };
                core.process (channels, 2, n);
            }

            for (auto v : l) check (std::isfinite (v) && std::abs (v) < 60.0f, "the output stays finite and bounded under an extreme input");
        }
    }
}

/** No lookahead, no oversampling: this module must never cost the host a
    sample of plugin-delay-compensation. */
void testLatencyIsAlwaysZero()
{
    OptoDsp dsp;
    dsp.prepare (kSampleRate, 512, 2);

    const float values[] { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
    check (dsp.latencyForParams (values, 5) == 0, "Crush 0 reports zero latency");

    const float loud[] { 100.0f, 24.0f, 1.0f, 1.0f, 1.0f };
    check (dsp.latencyForParams (loud, 5) == 0, "Crush 100, Stressed, Link, Color also reports zero latency");
}

} // namespace

//==============================================================================
int main()
{
    testQuietSignalIsLeftAlone();
    testReductionRisesWithCrush();
    testMakeupGainIsExact();
    testReleaseIsProgramDependent();
    testDistressorReleaseCeilingExceedsLa2a();
    testDistressorRatioExceedsLa2a();
    testStereoLink();
    testColorTogglesHarmonics();
    testTeleColorIsLocked();
    testStability();
    testLatencyIsAlwaysZero();

    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

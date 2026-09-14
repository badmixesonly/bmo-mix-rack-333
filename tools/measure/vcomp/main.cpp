/*
    Offline measurement harness for BMO Vcomp.

    Links the DSP core directly -- no plugin host, no GUI, no JUCE -- same
    shape as the other measure tools. Built for the listening pass, so every
    mode writes WAVs as well as numbers: the point is that a number and a
    listen are always of the same render.

      measure_vcomp curve
          The static curve at every AMOUNT: threshold, knee, ratio, the makeup
          it pays itself, and what a voice at the reference level actually
          comes out at. This is the table to look at first when the module
          feels wrong at one end of the knob -- the dead-bottom-third fault
          that testAmountGrabsHarder caught would have been obvious here.

      measure_vcomp presets [--outdir dir]
          Every factory preset rendered on the same voice, with its level
          delta printed. The deltas are the same figures VcompTests checks;
          the WAVs are so they can be heard against each other at a matched
          level, which is the only way to judge whether "Forward" and "In
          Front" are actually different settings or just two numbers.

      measure_vcomp gate [--outdir dir]
          A phrase into a quiet tail at a run of gate thresholds, with how much
          the tail was shut and whether the phrase was touched. The gate's
          whole risk is eating the front of a word, so the report prints the
          first 20 ms of the phrase separately.

      measure_vcomp bands [--outdir dir]
          What LOW THRU and HIGH THRU actually do: reconstruction error with
          the split in circuit and nothing compressing, and how much a steady
          tone is pumped by a loud burst with and without the split.

      measure_vcomp render [--in in.wav] --out out.wav [flags]
          Arbitrary WAV in, BMO out, every parameter as a flag -- so a result
          can sit beside a hand-bounced RVox/RComp/DC1A pass at a matched
          input. With no --in, uses the harness's own voice.

      measure_vcomp gen <voice|phrase|bands|sine> --out file.wav
          Exports the harness's own signals, so the *same* file can be fed
          through a competitor plugin rather than something only approximately
          alike.

    Flags for render: --amount pct --gate db --output db --complex 0|1
                      --attack ms --release ms --arc 0|1 --sidechain hz
                      --low hz --high hz
*/

#include "modules/vcomp/dsp/DspCore.h"
#include "tools/measure/Wav.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace bmo::vcomp;
using bmo::measure::readWav;
using bmo::measure::writeWav;

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;

//==============================================================================
// Signals. `voice` is the same shape tests/plugin/TestUtil.h uses, so a number
// printed here and a number printed by the plugin tests are about the same
// sound rather than about two things both called "a voice".
//==============================================================================

std::vector<float> sine (double hz, double seconds, double amplitude)
{
    const auto n = (size_t) (seconds * kSampleRate);
    std::vector<float> out (n);

    for (size_t i = 0; i < n; ++i)
        out[i] = (float) (amplitude * std::sin (2.0 * kPi * hz * (double) i / kSampleRate));

    return out;
}

/** The suite's test voice: a harmonic stack under a plucked envelope, gated
    into phrases, normalised to -18 dBFS RMS. Peaks around -3.6, a 14.4 dB
    crest -- which is the figure the makeup reference was chosen against. */
std::vector<float> voice (int samples)
{
    std::vector<float> out ((size_t) samples);
    double sumSquares = 0.0;

    for (int i = 0; i < samples; ++i)
    {
        const auto t = (double) i / kSampleRate;
        const auto beat = std::fmod (t, 0.55);
        const auto envelope = (std::fmod (t, 3.0) < 1.6 ? 1.0 : 0.0)
                            * (beat < 0.01 ? beat / 0.01 : std::exp (-(beat - 0.01) * 7.0));

        double sum = 0.0;

        for (int h = 1; h <= 120; ++h)
            sum += std::pow ((double) h, -1.4) * std::sin (2.0 * 2.0 * kPi * 75.0 * h * t);

        out[(size_t) i] = (float) (envelope * sum);
        sumSquares += (double) out[(size_t) i] * out[(size_t) i];
    }

    const auto rms = std::sqrt (sumSquares / (double) samples);
    const auto gain = rms > 0.0 ? std::pow (10.0, -18.0 / 20.0) / rms : 1.0;

    for (auto& v : out)
        v = (float) (v * gain);

    return out;
}

/** A loud phrase running into a quiet tail: what the gate is for. */
std::vector<float> phrase()
{
    auto out = voice ((int) (1.6 * kSampleRate));
    const auto tail = sine (220.0, 1.4, std::pow (10.0, -48.0 / 20.0));
    out.insert (out.end(), tail.begin(), tail.end());
    return out;
}

/** A steady low tone and a steady high tone under a burst in the middle of the
    band: what LOW THRU and HIGH THRU are for. */
std::vector<float> bandsSignal()
{
    constexpr double total = 3.0, burstFrom = 1.0, burstTo = 2.0;

    auto out = sine (80.0, total, std::pow (10.0, -20.0 / 20.0));
    const auto top = sine (12000.0, total, std::pow (10.0, -20.0 / 20.0));
    const auto burst = sine (2000.0, total, std::pow (10.0, -4.0 / 20.0));

    for (size_t i = 0; i < out.size(); ++i)
    {
        out[i] += top[i];

        if (i >= (size_t) (burstFrom * kSampleRate) && i < (size_t) (burstTo * kSampleRate))
            out[i] += burst[i];
    }

    return out;
}

//==============================================================================
std::vector<std::vector<float>> render (std::vector<std::vector<float>> channels,
                                        DspCore::Params params, double rate = kSampleRate)
{
    DspCore core;
    core.setParams (params);
    core.prepare (rate, 512, (int) channels.size());

    const auto frames = channels.empty() ? 0u : (unsigned) channels[0].size();
    std::vector<float*> pointers (channels.size());

    for (size_t at = 0; at < frames; at += 512)
    {
        const auto n = (int) std::min<size_t> (512, frames - at);

        for (size_t ch = 0; ch < channels.size(); ++ch)
            pointers[ch] = channels[ch].data() + at;

        core.process (pointers.data(), (int) channels.size(), n);
    }

    return channels;
}

std::vector<float> renderMono (const std::vector<float>& in, DspCore::Params p)
{
    return render ({ in }, p)[0];
}

double rms (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, x.size());
    double sum = 0.0;

    for (size_t i = from; i < to; ++i)
        sum += (double) x[i] * x[i];

    return to > from ? std::sqrt (sum / (double) (to - from)) : 0.0;
}

double peak (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, x.size());
    double m = 0.0;

    for (size_t i = from; i < to; ++i)
        m = std::max (m, (double) std::abs (x[i]));

    return m;
}

double magnitudeAt (const std::vector<float>& v, double hz, double fromSec, double toSec)
{
    const auto from = (size_t) (fromSec * kSampleRate);
    const auto to   = std::min (v.size(), (size_t) (toSec * kSampleRate));
    double re = 0.0, im = 0.0;

    for (size_t i = from; i < to; ++i)
    {
        const auto t = 2.0 * kPi * hz * (double) i / kSampleRate;
        re += (double) v[i] * std::cos (t);
        im += (double) v[i] * std::sin (t);
    }

    return 2.0 * std::sqrt (re * re + im * im) / (double) (to - from);
}

double db (double linear) { return 20.0 * std::log10 (std::max (linear, 1.0e-9)); }

size_t at (double seconds) { return (size_t) (seconds * kSampleRate); }

//==============================================================================
void printCurve()
{
    std::printf ("The static curve at each AMOUNT, and what it does to a tone at the\n"
                 "makeup reference (%.0f dBFS). 'out' should sit within a dB of the\n"
                 "reference at every setting -- that is the claim AMOUNT buys density\n"
                 "rather than level, and it is the first thing to check after a revoice.\n\n",
                 (double) kReferenceDb);

    std::printf ("%7s %10s %8s %8s %10s %10s %9s\n",
                 "amount", "thresh dB", "knee dB", "ratio", "makeup dB", "GR dB", "out dBFS");

    for (const auto amountPercent : { 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f,
                                      60.0f, 70.0f, 80.0f, 90.0f, 100.0f })
    {
        const auto curve = curveFor (amountPercent);

        // slope = 1 - 1/R, so R = 1/(1 - slope). Printed as the ratio rather
        // than the slope because the ratio is what anybody reading this is
        // thinking in; see core/dsp/GainComputer.h for why the type carries
        // the slope instead.
        const auto ratio = 1.0 / (1.0 - (double) curve.slope);

        auto p = DspCore::Params{};
        p.amountPercent = amountPercent;

        const auto in = sine (440.0, 2.0, std::pow (10.0, (double) kReferenceDb / 20.0));
        const auto out = renderMono (in, p);

        DspCore core;
        core.setParams (p);
        core.prepare (kSampleRate, 512, 1);
        auto probe = in;
        auto* q = probe.data();

        for (size_t a = 0; a + 512 <= probe.size(); a += 512)
        {
            q = probe.data() + a;
            core.process (&q, 1, 512);
        }

        std::printf ("%6.0f%% %10.1f %8.1f %8.2f %10.2f %10.2f %9.2f\n",
                     (double) amountPercent, (double) curve.thresholdDb, (double) curve.kneeDb,
                     ratio, (double) autoMakeupDb (curve),
                     (double) core.currentGainReductionDb(),
                     db (peak (out, at (1.0), at (2.0))));
    }
}

//==============================================================================
void printPresets (const std::string& outdir)
{
    const auto source = voice ((int) (6.0 * kSampleRate));
    const auto sourceDb = db (rms (source));

    std::printf ("\nEvery factory preset on the same voice (%.2f dBFS RMS in).\n"
                 "delta is what VcompTests checks against +/-3 dB; peak GR is how hard\n"
                 "the preset is actually working, which the delta deliberately hides.\n\n",
                 sourceDb);

    std::printf ("%-16s %10s %10s %10s\n", "preset", "delta dB", "peak GR", "out peak");

    struct Case { const char* name; DspCore::Params p; };

    auto make = [] (float amount, bool complex = false, float attack = kStandardAttackMs,
                    float release = kStandardReleaseMs, bool arc = kStandardArc,
                    float sidechain = kStandardSidechainHz,
                    float low = kStandardLowThruHz, float high = kStandardHighThruHz)
    {
        DspCore::Params p;
        p.amountPercent = amount;
        p.complex = complex;
        p.attackMs = attack;
        p.releaseMs = release;
        p.arc = arc;
        p.sidechainHz = sidechain;
        p.lowThruHz = low;
        p.highThruHz = high;
        return p;
    };

    // Kept in step with modules/vcomp/presets/FactoryPresets.h by hand, which
    // is a seam: this harness links the DSP core and not the module, so it
    // cannot read the real preset table without pulling JUCE in. If a preset
    // is added there and not here, this report is quietly incomplete -- check
    // the count against VcompTests, which does read the real table.
    const Case cases[]
    {
        { "Lift",           make (25.0f) },
        { "Forward",        make (55.0f) },
        { "In Front",       make (80.0f) },
        { "Fast Vocal",     make (65.0f, true, 0.8f, 90.0f, true, 120.0f) },
        { "Smooth Lead",    make (45.0f, true, 20.0f, 400.0f, true, 70.0f) },
        { "Keep The Chest", make (70.0f, true, kStandardAttackMs, kStandardReleaseMs,
                                  kStandardArc, kStandardSidechainHz, 160.0f) },
        { "Keep The Air",   make (70.0f, true, kStandardAttackMs, kStandardReleaseMs,
                                  kStandardArc, kStandardSidechainHz,
                                  kStandardLowThruHz, 6000.0f) },
        { "Manual",         make (50.0f, true, 5.0f, 150.0f, false) },
    };

    for (const auto& c : cases)
    {
        DspCore core;
        core.setParams (c.p);
        core.prepare (kSampleRate, 512, 1);

        auto out = source;
        auto worstGr = 0.0f;

        for (size_t a = 0; a < out.size(); a += 512)
        {
            auto* q = out.data() + a;
            core.process (&q, 1, (int) std::min<size_t> (512, out.size() - a));
            worstGr = std::max (worstGr, core.currentGainReductionDb());
        }

        std::printf ("%-16s %10.2f %10.2f %10.2f\n",
                     c.name, db (rms (out)) - sourceDb, (double) worstGr, db (peak (out)));

        if (! outdir.empty())
            writeWav (outdir + "/preset_" + std::string (c.name) + ".wav", { out }, kSampleRate);
    }

    if (! outdir.empty())
    {
        writeWav (outdir + "/preset_dry.wav", { source }, kSampleRate);
        std::printf ("\nwrote %zu WAVs to %s\n", sizeof (cases) / sizeof (cases[0]) + 1, outdir.c_str());
    }
}

//==============================================================================
void printGate (const std::string& outdir)
{
    const auto signal = phrase();

    std::printf ("\nThe gate, on a phrase running into a -48 dBFS tail at AMOUNT 75.\n"
                 "'tail' is what is left of the tail; 'onset' is the first 20 ms of the\n"
                 "phrase against the ungated render, and it is the number that matters --\n"
                 "a gate that cleans the silence and bites the first word is a bad trade.\n\n");

    std::printf ("%9s %12s %12s %12s\n", "gate dB", "tail dBFS", "shut dB", "onset dB");

    DspCore::Params base;
    base.amountPercent = 75.0f;

    const auto ungated = renderMono (signal, base);
    const auto ungatedTail = db (rms (ungated, at (2.4), at (3.0)));
    const auto ungatedOnset = db (peak (ungated, 0, at (0.02)));

    for (const auto gateDb : { kGateOffDb, -50.0f, -45.0f, -40.0f, -35.0f, -30.0f, -25.0f, -20.0f })
    {
        auto p = base;
        p.gateDb = gateDb;

        const auto out = renderMono (signal, p);
        const auto tail = db (rms (out, at (2.4), at (3.0)));
        const auto onset = db (peak (out, 0, at (0.02)));

        std::printf ("%9.1f %12.2f %12.2f %12.2f\n",
                     (double) gateDb, tail, tail - ungatedTail, onset - ungatedOnset);

        if (! outdir.empty())
            writeWav (outdir + "/gate_" + std::to_string ((int) gateDb) + ".wav", { out }, kSampleRate);
    }

    if (! outdir.empty())
        writeWav (outdir + "/gate_dry.wav", { signal }, kSampleRate);
}

//==============================================================================
void printBands (const std::string& outdir)
{
    std::printf ("\nLOW THRU and HIGH THRU.\n\n"
                 "Reconstruction: the split in circuit at AMOUNT 0, which must be flat --\n"
                 "the bands are meant to change what the compressor acts on, not to be an\n"
                 "EQ. Any figure here that is not near 0 means the crossover is not\n"
                 "summing, and the usual cause is the low band skipping the second\n"
                 "split's allpass (see Crossover.h).\n\n");

    std::printf ("%9s %12s\n", "hz", "error dB");

    {
        DspCore::Params p;
        p.complex = true;
        p.lowThruHz = 200.0f;
        p.highThruHz = 4000.0f;

        for (const auto hz : { 40.0, 80.0, 150.0, 200.0, 400.0, 1000.0, 2000.0,
                               4000.0, 8000.0, 14000.0 })
        {
            const auto in = sine (hz, 0.5, std::pow (10.0, -12.0 / 20.0));
            const auto out = renderMono (in, p);

            std::printf ("%9.0f %12.2f\n", hz,
                         db (rms (out, at (0.3), at (0.5))) - db (rms (in, at (0.3), at (0.5))));
        }
    }

    std::printf ("\nPumping: a steady 80 Hz and 12 kHz tone under a 2 kHz burst at AMOUNT 80.\n"
                 "The figure is how much the burst modulates each tone -- large and\n"
                 "negative means the compressor is riding it, near zero means the split\n"
                 "has taken it out of the compressor's reach.\n\n");

    std::printf ("%-22s %12s %12s\n", "setting", "80 Hz dB", "12 kHz dB");

    const auto signal = bandsSignal();

    struct Case { const char* name; float low, high; };

    const Case cases[]
    {
        { "both off (rails)", kLowThruOffHz, kHighThruOffHz },
        { "LOW THRU 300",     300.0f,        kHighThruOffHz },
        { "HIGH THRU 4000",   kLowThruOffHz, 4000.0f },
        { "both",             300.0f,        4000.0f },
    };

    for (const auto& c : cases)
    {
        DspCore::Params p;
        p.amountPercent = 80.0f;
        p.complex = true;
        p.lowThruHz = c.low;
        p.highThruHz = c.high;

        const auto out = renderMono (signal, p);

        const auto lowDucking = db (magnitudeAt (out, 80.0, 1.6, 1.9))
                                    - db (magnitudeAt (out, 80.0, 2.6, 2.9));
        const auto highDucking = db (magnitudeAt (out, 12000.0, 1.6, 1.9))
                                    - db (magnitudeAt (out, 12000.0, 2.6, 2.9));

        std::printf ("%-22s %12.2f %12.2f\n", c.name, lowDucking, highDucking);

        if (! outdir.empty())
            writeWav (outdir + "/bands_" + std::to_string ((int) c.low) + "_"
                          + std::to_string ((int) c.high) + ".wav", { out }, kSampleRate);
    }

    if (! outdir.empty())
        writeWav (outdir + "/bands_dry.wav", { signal }, kSampleRate);
}

//==============================================================================
bool flagValue (int argc, char** argv, const char* flag, std::string& out)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp (argv[i], flag) == 0)
        {
            out = argv[i + 1];
            return true;
        }

    return false;
}

float flagFloat (int argc, char** argv, const char* flag, float fallback)
{
    std::string s;
    return flagValue (argc, argv, flag, s) ? std::stof (s) : fallback;
}

std::string flagString (int argc, char** argv, const char* flag, const std::string& fallback = {})
{
    std::string s;
    return flagValue (argc, argv, flag, s) ? s : fallback;
}

} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    const std::string command = argc > 1 ? argv[1] : "";
    const auto outdir = flagString (argc, argv, "--outdir");

    if (command == "curve")
    {
        printCurve();
        return 0;
    }

    if (command == "presets") { printPresets (outdir); return 0; }
    if (command == "gate")    { printGate (outdir);    return 0; }
    if (command == "bands")   { printBands (outdir);   return 0; }

    if (command == "gen")
    {
        const std::string what = argc > 2 ? argv[2] : "";
        const auto outPath = flagString (argc, argv, "--out");

        if (outPath.empty())
        {
            std::printf ("gen needs --out file.wav\n");
            return 1;
        }

        std::vector<float> signal;

        if      (what == "voice")  signal = voice ((int) (6.0 * kSampleRate));
        else if (what == "phrase") signal = phrase();
        else if (what == "bands")  signal = bandsSignal();
        else if (what == "sine")   signal = sine (flagFloat (argc, argv, "--freq", 440.0f), 2.0,
                                                  std::pow (10.0, (double) flagFloat (argc, argv, "--amp", -10.0f) / 20.0));
        else
        {
            std::printf ("gen <voice|phrase|bands|sine>\n");
            return 1;
        }

        if (! writeWav (outPath, { signal }, kSampleRate))
        {
            std::printf ("could not write %s\n", outPath.c_str());
            return 1;
        }

        std::printf ("wrote %s\n", outPath.c_str());
        return 0;
    }

    if (command == "render")
    {
        const auto inPath = flagString (argc, argv, "--in");
        const auto outPath = flagString (argc, argv, "--out");

        if (outPath.empty())
        {
            std::printf ("render needs --out file.wav\n");
            return 1;
        }

        std::vector<std::vector<float>> dry;
        auto rate = kSampleRate;

        if (inPath.empty())
        {
            dry = { voice ((int) (6.0 * kSampleRate)) };
        }
        else if (! readWav (inPath, dry, rate))
        {
            std::printf ("could not read %s\n", inPath.c_str());
            return 1;
        }

        DspCore::Params p;
        p.amountPercent = flagFloat (argc, argv, "--amount", 0.0f);
        p.gateDb        = flagFloat (argc, argv, "--gate", kGateOffDb);
        p.outputDb      = flagFloat (argc, argv, "--output", 0.0f);
        p.complex       = flagFloat (argc, argv, "--complex", 0.0f) > 0.5f;
        p.attackMs      = flagFloat (argc, argv, "--attack", kStandardAttackMs);
        p.releaseMs     = flagFloat (argc, argv, "--release", kStandardReleaseMs);
        p.arc           = flagFloat (argc, argv, "--arc", kStandardArc ? 1.0f : 0.0f) > 0.5f;
        p.sidechainHz   = flagFloat (argc, argv, "--sidechain", kStandardSidechainHz);
        p.lowThruHz     = flagFloat (argc, argv, "--low", kStandardLowThruHz);
        p.highThruHz    = flagFloat (argc, argv, "--high", kStandardHighThruHz);

        const auto wet = render (dry, p, rate);

        if (! writeWav (outPath, wet, rate))
        {
            std::printf ("could not write %s\n", outPath.c_str());
            return 1;
        }

        std::printf ("wrote %s (%zu ch, %.0f Hz)\n", outPath.c_str(), wet.size(), rate);
        return 0;
    }

    std::printf ("usage: measure_vcomp <curve|presets|gate|bands|render|gen> [flags]\n"
                 "       see the comment at the top of tools/measure/vcomp/main.cpp\n");
    return 1;
}

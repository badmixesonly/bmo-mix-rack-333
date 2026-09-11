#pragma once

#include "modules/deq/dsp/Design.h"
#include "modules/deq/dsp/Dynamics.h"
#include "modules/deq/dsp/Svf.h"
#include <array>
#include <complex>

namespace bmo::deq
{

/** No hard ceiling in the design (spec A6). This is only the size of the
    fixed array, so the audio thread never allocates; the host-parameter
    budget -- 32 per rack slot -- is what actually limits a product, and
    params.h is where that gets decided. */
inline constexpr int kMaxBands = 64;

/** Coefficients are redesigned every this many samples and glide linearly
    in between. The cadence counts absolute samples from prepare()/reset(),
    never block boundaries, so the output is bit-identical whatever block size
    the host uses -- the spec's T1 check, and the reason it can hold. */
inline constexpr int kControlInterval = 8;

/** Static parameters glide to a new value over roughly this long. */
inline constexpr double kSmoothingMs = 10.0;

/** A dynamic band is only redesigned when its gain offset has moved by more
    than this since the last design. Static settings are always followed
    exactly; this only stops a band sitting in steady gain reduction from
    redesigning every interval over a thousandth of a dB of detector ripple. */
inline constexpr double kOffsetHysteresisDb = 0.001;

/** How bands combine. **Serial**, decided 2026-09-10 and pending a listening
    test; the measurements behind it are in modules/deq/spec/topology-options.md.

    - `serial`: each band's output feeds the next, as in a conventional
      parametric EQ. The total is the band curves added in dB, a low cut
      still cuts under an overlapping boost, and two -24 dB cuts give -48 dB.
    - `parallel` (the spec's original C4): out = x + sum(H_k x - x). Two
      coincident -24 dB cuts give -1.17 dB, polarity inverted, because
      1 + 2(G - 1) goes negative once G < 0.5.

    Parallel stays in the engine only so the listening test can A/B the two
    (`measure_deq render`). It is not meant to become a user control: a preset
    made in one would sound different in the other. Once the test confirms
    serial, delete it along with its tests.

    Both are zero latency -- a chain of IIR filters has no crossover and no
    delay line -- and cost the same. */
enum class Topology { serial, parallel };

/** Where a band acts on a stereo signal. `mid` and `side` blend from stereo
    (msAmount 0) to the chosen M/S channel alone (msAmount 1). */
enum class Placement { stereo, mid, side };

struct DynamicSettings
{
    bool      enabled     = false;
    double    thresholdDb = -24.0;
    double    ratio       = 2.0;
    double    kneeDb      = 6.0;
    double    rangeDb     = -6.0;     // where the gain can move to; sign = cut/boost
    Direction direction   = Direction::above;
    double    attackMs    = 5.0;      // tau convention, see Dynamics.h
    double    releaseMs   = 120.0;
    bool      rms         = false;
};

struct BandSettings
{
    bool      enabled     = false;
    Shape     shape       = Shape::bell;
    double    frequencyHz = 1000.0;
    double    q           = 0.707;
    double    gainDb      = 0.0;
    Placement placement   = Placement::stereo;
    double    msAmount    = 1.0;
    DynamicSettings dynamics;
};

struct Settings
{
    Topology topology = Topology::serial;
    std::array<BandSettings, kMaxBands> bands {};
};

//==============================================================================
/** The zero-latency dynamic EQ, JUCE-free. A ModuleDsp adapter maps a params.h
    value array onto Settings; tests and tools drive this directly.

    Per sample, per band:

    1. The band's coefficients step toward the latest design (Svf.h).
    2. Its detector reads a band-limited copy of the *dry* input through a
       sidechain filter of its own. It does not tap the band's filter: that
       filter's poles move with the band's gain (a bell's pole Q is A*Q), so
       a tapped detector would hear 0 dB at -12 dB of band gain and +12 dB at
       +12, and the dynamics would become a feedback loop.
    3. The band filters its input's M and S. Filtering L and R separately is
       the same thing -- H(L) = H(M) + H(S) -- so one pair of filters gives
       both the L/R contribution and the M/S one, and the M/S blend is exact at
       every value with no warm-up when it moves off an end.
    4. The contribution w = y - x is scaled by the band's enable fade and
       summed per the topology.

    Every control-interval samples, each band redesigns its target
    coefficients from its smoothed controls plus the detector's current gain
    offset. Nothing here delays the audio: latency is 0 in every mode.
*/
class DspCore
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels) noexcept;
    void reset() noexcept;

    /** Stores targets; cheap. The first call after prepare()/reset() is
        snapped to rather than glided to. */
    void setSettings (const Settings& s) noexcept { current = s; }
    const Settings& settings() const noexcept      { return current; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;
    void process (double* const* channels, int numChannels, int numSamples) noexcept;

    static constexpr int latencySamples() noexcept { return 0; }

    /** Deepest cut any dynamic band is applying right now, dB, >= 0. */
    double currentGainReductionDb() const noexcept;

    //== Inspection, for tests, tools and a future curve display ===============
    double sampleRate() const noexcept { return rate; }

    /** A band's design at its target static settings (no dynamic offset). */
    Biquad bandDesign (int band) const noexcept;

    /** The combined static response at the target settings, for a centred
        (L = R) source, per the topology. */
    std::complex<double> staticResponseAt (double hz) const noexcept;

    double bandOffsetDb (int band) const noexcept   { return bands[(size_t) band].offsetDb; }
    double bandEnvelope (int band) const noexcept   { return bands[(size_t) band].detector.envelope(); }
    double bandGainDb (int band) const noexcept     { return bands[(size_t) band].appliedGainDb; }

    /** No subnormal anywhere in filter or detector state. */
    bool allStateNormal() const noexcept;

    /** Every coefficient set in use, and every one being glided toward, is
        stable. Checked by the modulation tests at every step. */
    bool allCoefficientsStable() const noexcept;

private:
    /** A value that moves toward its target once per control tick, and in
        straight lines between ticks. */
    struct Glide
    {
        double target = 0.0, tick = 0.0, now = 0.0, step = 0.0;

        void snap (double v) noexcept { target = tick = now = v; step = 0.0; }
        void advanceTick (double alpha) noexcept
        {
            tick = target + alpha * (tick - target);
            if (std::abs (tick - target) < 1.0e-9) tick = target;
        }
    };

    struct Band
    {
        Glide logHz, logQ, gainDb, beta, enable;

        SvfCoeffs cur, next, step;   // in use, being glided to, per-sample increment
        SvfState  m, s;

        SvfCoeffs sideCoeffs;        // the detector's own sidechain filter
        SvfTaps   sideTaps;
        SvfState  sideM, sideS;
        Detector  detector;
        GainComputer computer;

        double offsetDb = 0.0, appliedGainDb = 0.0;
        bool   live = false;         // enabled, or still fading out

        // What `next` was designed from, so a static band is not redesigned.
        Shape  designedShape = Shape::bell;
        double designedHz = -1.0, designedQ = -1.0, designedStatic = 1.0e9, designedOffset = 0.0;
        double sideHz = -1.0, sideQ = -1.0; Shape sideShape = Shape::bell;
        double detAttack = -1.0, detRelease = -1.0; bool detRms = false;
    };

    template <typename Sample>
    void processImpl (Sample* const* channels, int numChannels, int numSamples) noexcept;

    void controlTick() noexcept;
    void resetBand (Band& b) noexcept;

    std::array<Band, kMaxBands> bands {};
    Settings current;
    DesignGrid grid;
    double rate = 48000.0, tickAlpha = 0.0;
    int tickPhase = 0;
    bool primed = false;
};

} // namespace bmo::deq

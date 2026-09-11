#pragma once

#include "modules/tune/dsp/LatencyContract.h"
#include "modules/tune/dsp/SincTable.h"
#include <array>
#include <vector>

namespace bmo::tune
{

/** The HYBRID engine: pitch-synchronous overlap-add (spec §6).

    Grains two analysis periods long, Hann-windowed, are taken one detected
    period apart and overlap-added at synthesis marks spaced by the *target*
    period. Raising pitch places grains closer together than they were taken,
    so some cycles are used twice; lowering drops some. Output is normalised
    by the summed window, so any spacing reconstructs at unity.

    Formants. A grain that is only repositioned keeps its spectral envelope,
    which is why PSOLA preserves formants (spec §6.1) -- measured here at a
    formant scale of 0.997 to 1.000 across a semitone either way, inside the
    spec's 2 % gate (tests/dsp/HybridTests.cpp). Reading each grain at rate
    phi while the marks stay at the target period moves the envelope by phi
    and leaves the pitch alone, which is Formant Shift. So:

      Formant Correct on   grains read at phi (1 unless shifted): formants
                           stay where the singer put them
      Formant Correct off  grains read at rho x phi: formants move with the
                           pitch, CLASSIC's brighter sound with HYBRID's
                           transitions

    Spec §6.2 does this with an LPC inverse/resynthesis stage instead. That
    stage was built (modules/tune/dsp/Lpc.h, tested) and taken out of the
    signal path: at the host rate an order-24 predictor spent its poles on
    the empty band up to Nyquist and modelled no formants at all, and mapping
    a 16 kHz envelope up to the host rate produced a predictor too
    ill-conditioned to survive the grains' interpolation. modules/tune/AGENTS.md
    records what would bring it back.

    Latency: the same contract as CLASSIC (LatencyContract.h). Spec §6.1
    budgets a period of lookahead, because grain positions come from a
    pitch-mark detector that has to see each mark first (§7). They do not
    need to: the analysis position advances by exactly one detected period
    per grain, so neighbouring grains are always one cycle apart -- which is
    what pitch-synchronous means; the absolute phase never matters. Grains are
    then read as the input arrives, the only hard floor is the kernel's
    lookahead, and Live keeps each grain's delay in [rest, rest + T] exactly
    as CLASSIC keeps its read. An idle note sits on the rest delay exactly.
*/
class HybridEngine
{
public:
    /** Allocates. */
    void prepare (double sampleRate, double longestPeriod);
    void reset();

    void setLatencyMode (bool studio, double rangePeriod) noexcept;
    int reportedLatency() const noexcept { return studio ? contract::studio (rangePeriodCurrent) : 0; }

    void setFormant (bool correct, double shiftCents) noexcept;

    /** One sample. `settled` = unvoiced and correction faded out, when the
        engine goes home to its rest delay. */
    float process (float input, double correctionCents, double period, bool settled) noexcept;

    /** While CLASSIC is playing: the input history stays current and the
        grains stand down, so a switch here starts from real audio. */
    void feed (float input, double period) noexcept;

    //== For the analysis dump and the tests ===================================
    double currentLag() const noexcept { return lastDelay; }
    double currentRatio() const noexcept { return ratio; }
    long long grainsScheduled() const noexcept { return scheduled; }

private:
    struct Grain
    {
        double centreOut = 0.0;   ///< output time of the grain's centre
        double centreIn = 0.0;    ///< input time it was taken from
        double halfOut = 1.0;     ///< half-length at the output
        double rate = 1.0;        ///< input samples read per output sample
    };

    void scheduleGrains (double period) noexcept;

    double fs = 48000.0;
    long long now = -1;

    std::vector<float> input;
    int mask = 0;
    SincBank kernels;

    std::array<Grain, 8> grains {};
    int grainCount = 0;
    double nextSynthesis = 0.0, nextAnalysis = 0.0, heldPeriod = 240.0;
    bool started = false;
    long long scheduled = 0;

    bool studio = false;
    double rangePeriodCurrent = 600.0;
    double ratio = 1.0, lastDelay = contract::kLiveRest;
    bool settledNow = false;

    bool formantCorrect = true;
    double formantRatio = 1.0;
};

} // namespace bmo::tune

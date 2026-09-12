#pragma once

#include "modules/tune/dsp/LatencyContract.h"
#include "modules/tune/dsp/SincTable.h"
#include <array>
#include <vector>

namespace bmo::tune
{

/** The CLASSIC engine: a fractional-rate reader over the input with whole
    cycles repeated or deleted to keep it inside its delay window (spec §5).

    The read pointer is carried as a delay behind the newest input sample,
    `lag`. Reading at rate rho moves it by 1 - rho per sample: raising pitch
    shrinks the lag, lowering grows it. When it leaves its window it jumps
    by a whole number of detected periods, which lands it on the same point
    of the waveform one cycle away -- that is the patent's "exactly one cycle
    period will be subtracted from the output pointer" -- and the jump is
    crossfaded, linearly, over half a period (spec §5.2: the two reads are
    pitch-synchronous and so correlated, and an equal-power fade on
    correlated material bumps the level mid-fade).

    The window is the latency contract, Live (LatencyContract.h): [floor,
    rest + T]. At rest the engine sits contract::kLiveRestMs behind the newest
    sample -- 4 ms since 2026-09-11, 192 samples at 48 kHz -- and is reported
    to the host as 0; while correcting it wanders up to a period later still.
    This is the Waves contract (spec §2, §0.1).

    KNOWN BROKEN (2026-09-11 review): `hi = rest + T` is an absolute delay of
    rest + T, so at the bottom of the range the read runs later than Waves
    Tune Real-Time, which the latency rule forbids -- 15.3 ms at E2 against
    the 10.62 ms ceiling, on 54 cells of bmo-tune-latency's table. Waves
    itself does not do this: its correcting delay (7.33-10.62 ms) barely
    exceeds its in-tune delay (5.93-10.50). The upper bound wants to be an
    excursion above the rest, not a whole period on top of an already-4 ms
    rest. testing-notes/tune-latency-review-2026-09-11.md.

    Whenever the input is unvoiced and correction has faded out, the engine
    homes back to its rest position with an equal-power crossfade (the two
    reads are then uncorrelated noise, where equal-power is the right law),
    so a Live instance does not carry a period of extra delay into the next
    phrase just because the last one ended on a correction.

    Formants move with the pitch here, by design (spec §5.3). Do not fix it.
*/
class ClassicEngine
{
public:
    using Sinc = SincTable<32>;

    /** Delay below which a read would need samples not yet written. */
    static constexpr int kFloor = contract::kFloor;

    /** Where Live rests at the prepared rate: contract::kLiveRestMs behind
        the newest sample, and never nearer than two samples above the floor.

        Resting on the floor itself meant any upward correction at all --
        including the few ten-thousandths of a cent of detector noise on an
        in-tune note -- spliced a whole period of delay in at the first voiced
        sample. Two samples above it cured that and no more: the read still
        had only about one period of room either way, and a correction held
        against a singer who had moved spent it and spliced. Those were round
        three's pops; LatencyContract.h has what each rest measured.

        The rest is also what aligns the read with the detector's estimate,
        which is the larger of its two jobs and was not known when it was
        chosen: see LatencyContract.h. */
    int liveRest() const noexcept { return rest; }

    /** Allocates. `longestPeriod` is the largest period any pitch range can
        report, in samples. */
    void prepare (double sampleRate, double longestPeriod);
    void reset();

    /** One sample. `period` is the detector's held period (0 if it has
        never had one); `settled` says correction has fully faded and the
        input is unvoiced, which is when homing is allowed. */
    float process (float input, double correctionCents, double period, bool settled) noexcept;

    //== For the analysis dump and the tests ===================================
    double currentLag() const noexcept { return lag; }
    double currentRatio() const noexcept { return ratio; }
    bool splicedThisSample() const noexcept { return spliced; }
    long long spliceCount() const noexcept { return splices; }

private:
    double read (const Sinc&, double lagBehindNewest) const noexcept;
    void startFade (double newLag, int length, bool equalPower) noexcept;

    double fs = 48000.0;
    std::vector<float> ring;
    int mask = 0, write = 0;

    SincBank kernels;   // full band while aliases stay above 20 kHz; see SincBank

    int rest = contract::kLiveFloorRest;   // set by prepare(), from the rate
    double lag = (double) contract::kLiveFloorRest, ratio = 1.0, lastPeriod = 0.0;

    // Crossfade state: the outgoing read and its progress.
    bool fading = false, fadeEqualPower = false;
    double fadeLag = 0.0;
    int fadeLength = 0, fadePosition = 0;

    bool spliced = false;
    long long splices = 0;
};

} // namespace bmo::tune

#pragma once

#include "modules/tune/dsp/SincTable.h"

namespace bmo::tune
{

/** The latency contract (spec §0.1, §2), in one place: Live, the Waves
    contract. The plugin reports 0 to the host and runs a hair behind -- the
    32-tap kernel's lookahead -- at rest, and up to a period later while it
    corrects.

    There was a Studio contract too, a fixed delay reported as PDC. It went
    with HYBRID on 2026-09-11 when Tune RT became Live only; it is on branch
    archive/hybrid-studio, and testing-notes/nrt-tune-handoff-2026-09-11.md
    has its maths and its measurements.
*/
namespace contract
{
    /** Delay below which a 32-tap read would need a sample not yet written. */
    inline constexpr int kFloor = SincTable<32>::kLookahead + 1;

    /** The least rest that works at all: two samples of headroom above the
        floor, so detector noise on an in-tune note does not splice a period
        in (see ClassicEngine's history). 19 samples, 0.40 ms at 48 kHz. */
    inline constexpr int kLiveFloorRest = kFloor + 2;

    /** Where Live rests, as a delay behind the newest sample.

        It used to be kLiveFloorRest and no more, which left the read about
        one period of room: a correction held against a singer who had moved
        drifted out of the window and spliced a whole period, which is what
        Frosty heard in round three as pops at retune 20 ms (2026-09-11, see
        testing-notes/shootout-2026-09-11.md).

        Resting further back widens the window both ways. Measured on Failure
        at retune 20 ms: 120 splices at 0.40 ms, 50 at 2 ms, 35 at 4 ms, 31 at
        6 -- and the correction lag falls with it, 6.21 ms mean to 5.16, 3.19,
        1.20. Frosty chose 4 ms (2026-09-11): where the splice curve flattens,
        and level with Auto-Tune Artist's measured 6.49 ms of true latency
        rather than merely inside Waves' 10.62 ms ceiling (the latency rule).

        In milliseconds, not samples, so every rate rests at the same delay --
        both shoot-out takes are 44.1 kHz, not 48. */
    inline constexpr double kLiveRestMs = 4.0;

    /** That rest in samples at this rate, never below kLiveFloorRest. */
    inline int liveRestSamples (double fs) noexcept
    {
        const auto n = (int) (kLiveRestMs * 0.001 * fs + 0.5);
        return n > kLiveFloorRest ? n : kLiveFloorRest;
    }
}

} // namespace bmo::tune

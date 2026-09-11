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

    /** Where Live rests: two samples of headroom above the floor, so detector
        noise on an in-tune note does not splice a period in (see
        ClassicEngine::kLiveRest's history). 19 samples, 0.40 ms at 48 kHz. */
    inline constexpr int kLiveRest = kFloor + 2;
}

} // namespace bmo::tune

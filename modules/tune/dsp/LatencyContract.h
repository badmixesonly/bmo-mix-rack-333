#pragma once

#include "modules/tune/dsp/SincTable.h"
#include <cmath>

namespace bmo::tune
{

/** The latency contract both engines keep (spec §0.1, §2), in one place.

    Both engines read the input through the same 32-tap kernel and keep their
    read inside a window of whole periods, so they share a floor and a
    Studio figure. Sharing the Studio figure is deliberate: a host is told
    the plugin's latency, and switching engines must not change it -- a PDC
    change mid-session is a timing jump on the whole track.
*/
namespace contract
{
    /** Delay below which a 32-tap read would need a sample not yet written. */
    inline constexpr int kFloor = SincTable<32>::kLookahead + 1;

    /** Where Live rests: two samples of headroom above the floor, so detector
        noise on an in-tune note does not splice a period in (see
        ClassicEngine::kLiveRest's history). */
    inline constexpr int kLiveRest = kFloor + 2;

    /** The correction a window is sized for; the engines are documented as
        accurate to +/-400 cents (spec §6.1). */
    inline constexpr double kDesignCents = 400.0;

    /** How far below its window a read can drift while a splice or a grain is
        in flight, as a fraction of the period. The larger of CLASSIC's
        (half a period of crossfade at the design ratio: 0.13) and HYBRID's
        (a grain resampled at the design ratio reads ahead by 1 - 1/rho of
        its half-length: 0.21), so one figure covers both. */
    inline double guardFraction() noexcept
    {
        const auto rho = std::exp2 (kDesignCents / 1200.0);
        const auto classic = 0.5 * (rho - 1.0);
        const auto hybrid = 1.0 - 1.0 / rho;
        return classic > hybrid ? classic : hybrid;
    }

    /** The fixed Studio delay for a range whose longest period is
        `rangePeriod` samples: the window's centre, half a period plus the
        guard above the floor. */
    inline int studio (double rangePeriod) noexcept
    {
        return kFloor + (int) std::ceil (rangePeriod * (0.5 + guardFraction()));
    }
}

} // namespace bmo::tune

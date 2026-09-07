#pragma once

#include "modules/opto/params.h"
#include <vector>

namespace bmo::opto
{

/** The LEVEL figures here match each preset's loudness to Init's, the way
    modules/AGENTS.md asks for -- against the current curve (ratio/knee fixed
    per mode; these presets don't set kMode or kColor, so they run Tele with
    its always-on Color).

    Re-solved in 0.2.0, when Tele started delivering a genuine 3:1 instead of
    the 1.67:1 its feedback loop had been quietly producing (see
    feedbackSlope() in Detector.h). Every preset that leans on Tele got
    correspondingly quieter, and CI measured exactly how much: Vocal Glue was
    6.03dB short and moves 5.5 -> 11.53dB, Crushed <3 was 13.51dB short.
    Gentle was still inside tolerance and is untouched. LEVEL is a plain
    gain, so these are arithmetic, not estimates.

    **Crushed <3 cannot be fully matched any more.** It wants 26.21dB of
    makeup and kLevel's range stops at 24 -- a range that is permanent per
    the repo's AGENTS.md, so it is the preset that has to give, not the
    parameter. It sits at the 24.0 rail and lands 2.2dB quiet, inside the
    level-matching test's +/-3dB but with under 1dB of headroom, so the next
    change that deepens Tele's reduction will break it again. The fix at that
    point is to pull its CRUSH back from 85%, which is a decision about how
    crushed "Crushed" should be -- ask, don't just turn it down.

    A source-dependent auto-makeup was considered here in 0.2.0 and
    **rejected**: it would have made all of this moot, but neither the LA-2A
    nor the Distressor has one, and Frosty chose to keep LEVEL the hand-set
    makeup the hardware actually has. See params.h.

    None of these are ear-tuned against real programme material -- Frosty may
    want to hand-author these values himself going forward rather than have
    them back-solved; ask before guessing again if this file needs touching.
    See tests/plugin/OptoTests.cpp for the level-matching test either way --
    it stays useful as a regression tripwire regardless of who picks the
    numbers. */
inline const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        { "Gentle",     { { kCrush, 15.0f }, { kLevel, 3.2f } } },
        { "Vocal Glue", { { kCrush, 45.0f }, { kLevel, 11.53f } } },
        { "Crushed <3", { { kCrush, 85.0f }, { kLevel, 24.0f } } },   // at the rail, 2.2dB quiet -- see above
    };

    return presets;
}

} // namespace bmo::opto

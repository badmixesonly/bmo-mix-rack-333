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

    **Through 0.2.0, Crushed <3 could not be fully matched** -- history now,
    but it explains the shape of everything below. It wanted 26.21dB of
    makeup where kLevel's range stops at 24 -- a range that is permanent per
    the repo's AGENTS.md, so it was the preset that had to give, not the
    parameter. It sat at the 24.0 rail and landed 2.2dB quiet, inside the
    level-matching test's +/-3dB but with under 1dB of headroom.

    **0.2.1 did not ease that -- it dissolved it.** The release fix (see
    Detector.h) means the cells hold far less sustained reduction, so every
    preset needs *less* makeup than it did, and CI measured how much: with
    LEVEL still at the old 24.0, Crushed came out **3.64dB loud** on both
    platforms. It needs **20.36dB**, not 26.21. The +24 rail it has been
    pinned against for two sessions now has 3.6dB of clear headroom, and it
    got there as a side effect of fixing the release rather than from
    anything aimed at the level.

    The estimate that preceded that measurement was badly wrong -- the local
    solver put the saving at ~1.15dB against an actual ~5.9dB. Treat
    tools/measure/renders' preset mode as a way to rank options and nothing
    more; the value comes from CI.

    **Depth is still a real decision, and it is now affordable.** Deeper
    reduction is a quieter output and therefore *more* makeup, not less --
    depth and the rail pull in opposite directions, which is the arithmetic
    that ruled 12-15dB out while the preset needed 26dB. At 20.36dB there is
    room again. CRUSH stays at 85 for now because that was decided when the
    headroom was thought to be 1dB, not 3.6, and the release fix has changed
    how crushed this preset *feels* independently of how deep it goes -- so
    it deserves an ear pass before another number is chosen. If it is wanted
    deeper, CRUSH 100 is the direct route, ELD mode reaches the same depth
    for ~1.7dB less makeup at the cost of being a different unit's
    character, and kLevel's range no longer needs reopening.

    **Gentle and Vocal Glue drifted too** and are still carrying values
    solved against the old release. They pass the +/-3dB check, so CI never
    said by how much -- BMO_PRINT_PRESET_LEVELS is set on the workflow's
    Test step to find out. Their values come from that, never from the local
    solver, which ports voice() faithfully but omits the drive and Color
    stages and lands ~8dB out at deep settings.

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
        { "Crushed <3", { { kCrush, 85.0f }, { kLevel, 20.36f } } },  // measured on CI, run 34084188862
    };

    return presets;
}

} // namespace bmo::opto

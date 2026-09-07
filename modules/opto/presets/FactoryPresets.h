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
    parameter. It sat at the 24.0 rail and landed 2.2dB quiet, inside the
    level-matching test's +/-3dB but with under 1dB of headroom.

    **0.2.1 eased that rather than solving it.** The release fix (see
    Detector.h) means the cells hold far less sustained reduction, so every
    preset here needs *less* makeup than it did -- Crushed's shortfall drops
    to roughly 1dB. The rail is no longer one change away from breaking.

    Frosty asked for it to crush harder, at 12-15dB, and then chose not to
    once the arithmetic was straight: **depth and the rail pull in opposite
    directions.** Deeper reduction is a quieter output and therefore *more*
    makeup, not less. 12-15dB on real material needs CRUSH near 100, which
    lands the preset ~4.5dB quiet and fails the level-match check outright.
    So CRUSH stays at 85 and the preset banks the headroom instead. If it
    ever needs to be genuinely deeper, the honest routes are ELD mode (the
    10:1 reaches the same depth for ~1.7dB less makeup, but it is a
    different unit's character) or reopening kLevel's range -- not nudging
    this number.

    **All three LEVEL values below still need re-solving**, because the
    release change moved every one of them. They come from CI with
    BMO_PRINT_PRESET_LEVELS set; the local solver in tools/measure/renders
    ports voice() faithfully but omits the drive and Color stages and lands
    ~8dB out at deep settings, so it can rank options and must not set a
    value.

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
        { "Crushed <3", { { kCrush, 85.0f }, { kLevel, 24.0f } } },   // needs re-solving from CI -- see above
    };

    return presets;
}

} // namespace bmo::opto

#pragma once

#include "modules/opto/params.h"
#include <vector>

namespace bmo::opto
{

/** The LEVEL figures here match each preset's loudness to Init's, the way
    modules/AGENTS.md asks for -- against the current curve (ratio/knee fixed
    per mode; these presets don't set kMode or kColor, so they run Tele with
    its always-on Color). Gentle and Vocal Glue's hand estimates happened to
    land inside CI's +/-3dB level-matching tolerance already; Crushed <3's
    didn't (was 8.0dB, measured 4.67dB short), so 12.7dB here is back-solved
    exactly from that measurement, same as the process used the very first
    time these presets were tuned. None of these are ear-tuned against real
    programme material -- Frosty may want to hand-author these values
    himself going forward rather than have them estimated/back-solved; ask
    before guessing again if this file needs touching. See
    tests/plugin/OptoTests.cpp for the level-matching test either way -- it
    stays useful as a regression tripwire regardless of who picks the
    numbers. */
inline const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        { "Gentle",     { { kCrush, 15.0f }, { kLevel, 3.2f } } },
        { "Vocal Glue", { { kCrush, 45.0f }, { kLevel, 5.5f } } },
        { "Crushed <3", { { kCrush, 85.0f }, { kLevel, 12.7f } } },
    };

    return presets;
}

} // namespace bmo::opto

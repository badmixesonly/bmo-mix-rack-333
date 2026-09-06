#pragma once

#include "modules/opto/params.h"
#include <vector>

namespace bmo::opto
{

/** The LEVEL figures here match each preset's loudness to Init's, the way
    modules/AGENTS.md asks for. The first pass was a guess at how much the
    curve reduces at that CRUSH setting on a typical programme and undershot
    badly at higher Crush (Crushed <3 was over 10 dB short) -- these are
    back-solved from tests/plugin/OptoTests.cpp's own level-matching test,
    which measures the actual RMS loss on its "voice" signal and is exact
    because LEVEL is a post-detector multiply (it cannot change how much the
    cell reduces, only what happens after). Still not tuned by ear against
    real programme material -- see tests/plugin/OptoTests.cpp for the test
    these have to pass. */
inline const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        { "Gentle",     { { kCrush, 15.0f }, { kLevel, 3.2f } } },
        { "Vocal Glue", { { kCrush, 45.0f }, { kLevel, 7.2f } } },
        { "Crushed <3", { { kCrush, 85.0f }, { kLevel, 13.6f } } },
    };

    return presets;
}

} // namespace bmo::opto

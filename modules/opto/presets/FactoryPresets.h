#pragma once

#include "modules/opto/params.h"
#include <vector>

namespace bmo::opto
{

/** The LEVEL figures here match each preset's loudness to Init's, the way
    modules/AGENTS.md asks for. The previous pass (3.2 / 7.2 / 13.6 dB) was
    back-solved exactly from CI's own measurement -- but against the old
    curve, which swept ratio up to 9:1 and narrowed the knee to 6 dB at high
    Crush. Now that ratio and knee are fixed per mode (Tele: 3:1, 16 dB --
    see Detector.h), these presets (which don't set kMode or kColor, so they
    run Tele with its always-on Color) compress noticeably less at the same
    Crush than before, especially Crushed <3. These are a rough hand
    estimate for the new curve, not measured -- see
    tests/plugin/OptoTests.cpp for the level-matching test they have to
    pass, and back-solve again from its failure output the way the last
    pass was, rather than trusting these numbers. */
inline const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        { "Gentle",     { { kCrush, 15.0f }, { kLevel, 3.2f } } },
        { "Vocal Glue", { { kCrush, 45.0f }, { kLevel, 5.5f } } },
        { "Crushed <3", { { kCrush, 85.0f }, { kLevel, 8.0f } } },
    };

    return presets;
}

} // namespace bmo::opto

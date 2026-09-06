#pragma once

#include "modules/opto/params.h"
#include <vector>

namespace bmo::opto
{

/** The LEVEL figures here are a starting guess at matching each preset's
    loudness to Init's, the way modules/AGENTS.md asks for -- picked from how
    much the curve reduces at that CRUSH setting on a typical programme, not
    measured against a reference the way BMO Saturator's voicing was. Retune
    by ear once this is loaded somewhere real; see
    tests/plugin/OptoTests.cpp for the level-matching test these have to
    pass. */
inline const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        { "Gentle",     { { kCrush, 15.0f }, { kLevel, 0.0f } } },
        { "Vocal Glue", { { kCrush, 45.0f }, { kLevel, 1.5f } } },
        { "Crushed <3", { { kCrush, 85.0f }, { kLevel, 3.0f } } },
    };

    return presets;
}

} // namespace bmo::opto

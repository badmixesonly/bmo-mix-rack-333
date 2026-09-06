#pragma once

#include "core/state/ParamSpec.h"

namespace bmo::opto
{

//==============================================================================
// Parameter IDs. Permanent and append-only -- see modules/eq/params.h for why,
// and tests/plugin/OptoTests.cpp for the table that holds them.
//==============================================================================

inline constexpr auto kModuleId   = "opto";
inline constexpr auto kModuleName = "BMO Opto";

// Two controls, on purpose: the real hardware this is modelled on has no
// threshold, ratio, attack or release knobs either -- see DspCore.h for what
// each one actually drives.
inline constexpr auto kCrush = "crush";
inline constexpr auto kLevel = "level";

enum Index { crush, level, count };

inline constexpr int kVersionHint   = 1;
inline constexpr int kStateVersion  = 1;
inline constexpr int kSchemaVersion = 1;

inline const ParamSpecs& specs()
{
    using S = ParamSpec;
    using F = ParamFormat;

    static const ParamSpecs s
    {
        // CRUSH <3: how hard the programme drives the cell. Not a threshold --
        // turning it up both lowers the effective threshold and raises the
        // effective ratio at once, the way the real Peak Reduction knob does.
        S::floatParam (kCrush, "Crush", 0.0f, 100.0f, 0.1f, 35.0f, F::Percent),

        // LEVEL: makeup gain after the cell. Not automatic -- the ear sets it.
        S::floatParam (kLevel, "Level", -24.0f, 24.0f, 0.01f, 0.0f, F::Decibels),
    };

    return s;
}

} // namespace bmo::opto

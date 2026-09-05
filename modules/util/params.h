#pragma once

#include "core/state/ParamSpec.h"

namespace bmo::util
{

//==============================================================================
// Parameter IDs. Permanent and append-only from day one -- see
// modules/eq/params.h for why, and tests/plugin/UtilTests.cpp for the table.
//==============================================================================

inline constexpr auto kModuleId   = "util";
inline constexpr auto kModuleName = "BMO Util";

inline constexpr auto kGain   = "gain";
inline constexpr auto kPan    = "pan";
inline constexpr auto kWidth  = "width";
inline constexpr auto kPhaseL = "phase_l";
inline constexpr auto kPhaseR = "phase_r";
inline constexpr auto kMono   = "mono";

enum Index { gain, pan, width, phaseL, phaseR, mono, count };

inline constexpr int kVersionHint   = 1;
inline constexpr int kStateVersion  = 1;
inline constexpr int kSchemaVersion = 1;

inline const ParamSpecs& specs()
{
    using S = ParamSpec;
    using F = ParamFormat;

    static const ParamSpecs s
    {
        S::floatParam (kGain,  "Gain",  -24.0f,  24.0f, 0.01f,   0.0f, F::Decibels),
        S::floatParam (kPan,   "Pan",  -100.0f, 100.0f, 1.0f,    0.0f, F::Pan),
        S::floatParam (kWidth, "Width",   0.0f, 200.0f, 1.0f,  100.0f, F::Percent),
        S::boolParam  (kPhaseL, "Phase L", false),
        S::boolParam  (kPhaseR, "Phase R", false),
        S::boolParam  (kMono,   "Mono",    false),
    };

    return s;
}

} // namespace bmo::util

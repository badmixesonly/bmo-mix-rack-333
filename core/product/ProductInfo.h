#pragma once

#include "core/state/PresetManager.h"

namespace bmo
{

/** What distinguishes one product from another, beyond the module it runs:
    the name in the header, where its presets go, and its version tags. */
struct ProductInfo
{
    juce::String name;      ///< "BMO EQ"; in the header and the window title
    PresetInfo presets;
    int versionHint  = 1;   ///< juce::ParameterID version hint, per product
    int stateVersion = 1;   ///< written into saved state for future migration
};

} // namespace bmo

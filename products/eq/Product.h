#pragma once

#include "core/product/SingleModuleProcessor.h"
#include "modules/eq/Module.h"
#include "modules/eq/params.h"

namespace bmo::products
{

/** BMO EQ, formerly FrostyEQ. The plugin code and bundle ID are the old
    ones, so sessions saved with FrostyEQ open with this; the preset folder is
    the new name, with the old one migrated on first run. */
inline ProductInfo eqInfo()
{
    return { "BMO EQ",
             { "BMO EQ", ".bmoeq", "FrostyEQ", ".frostyeq" },
             eq::kVersionHint, eq::kStateVersion };
}

inline std::unique_ptr<SingleModuleProcessor> createEq()
{
    return std::make_unique<SingleModuleProcessor> (eq::module(), eqInfo());
}

} // namespace bmo::products

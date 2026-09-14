#pragma once

#include "core/product/SingleModuleProcessor.h"
#include "modules/vcomp/Module.h"
#include "modules/vcomp/params.h"

namespace bmo::products
{

inline ProductInfo vcompInfo()
{
    return { "BMO Vcomp",
             { "BMO Vcomp", ".bmovcomp", {}, {} },
             vcomp::kVersionHint, vcomp::kStateVersion };
}

inline std::unique_ptr<SingleModuleProcessor> createVcomp()
{
    return std::make_unique<SingleModuleProcessor> (vcomp::module(), vcompInfo());
}

} // namespace bmo::products

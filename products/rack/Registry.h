#pragma once

#include "core/product/ModuleDef.h"
#include <vector>

namespace bmo::products
{

/** Every module the rack can hold, in the order the menu lists them.
    A new module goes here and nowhere else in the rack. */
const std::vector<const ModuleDef*>& registry();

} // namespace bmo::products

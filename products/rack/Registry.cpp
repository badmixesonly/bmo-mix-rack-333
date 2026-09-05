#include "Registry.h"
#include "modules/eq/Module.h"
#include "modules/sat/Module.h"
#include "modules/util/Module.h"

namespace bmo::products
{

const std::vector<const ModuleDef*>& registry()
{
    static const std::vector<const ModuleDef*> defs {
        &util::module(),
        &eq::module(),
        &sat::module(),
    };

    return defs;
}

} // namespace bmo::products

#include "Module.h"
#include "modules/util/dsp/UtilDsp.h"
#include "modules/util/panel/UtilPanel.h"
#include "modules/util/params.h"
#include "modules/util/presets/FactoryPresets.h"

namespace bmo::util
{

const ModuleDef& module()
{
    static const ModuleDef def {
        kModuleId, kModuleName, kSchemaVersion,
        160, juce::Colour (0xff7fc98a),
        specs(), factory(),
        [] { return createDsp(); },
        [] (ui::ModuleContext ctx) -> std::unique_ptr<ui::ModulePanel>
        {
            return std::make_unique<UtilPanel> (std::move (ctx));
        },
    };

    return def;
}

} // namespace bmo::util

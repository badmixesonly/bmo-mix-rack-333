#include "Module.h"
#include "modules/eq/dsp/EqDsp.h"
#include "modules/eq/panel/EqPanel.h"
#include "modules/eq/params.h"
#include "modules/eq/presets/FactoryPresets.h"

namespace bmo::eq
{

const ModuleDef& module()
{
    static const ModuleDef def {
        kModuleId, kModuleName, kSchemaVersion,
        280, juce::Colour (0xfff08cb4),
        specs(), factory(),
        [] { return createDsp(); },
        [] (ui::ModuleContext ctx) -> std::unique_ptr<ui::ModulePanel>
        {
            return std::make_unique<EqPanel> (std::move (ctx));
        },
    };

    return def;
}

} // namespace bmo::eq

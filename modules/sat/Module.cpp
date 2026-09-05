#include "Module.h"
#include "modules/sat/dsp/SatDsp.h"
#include "modules/sat/panel/SatPanel.h"
#include "modules/sat/params.h"
#include "modules/sat/presets/FactoryPresets.h"

namespace bmo::sat
{

const ModuleDef& module()
{
    static const ModuleDef def {
        kModuleId, kModuleName, kSchemaVersion,
        260, juce::Colour (0xffefa552),
        specs(), factory(),
        [] { return createDsp(); },
        [] (ui::ModuleContext ctx) -> std::unique_ptr<ui::ModulePanel>
        {
            return std::make_unique<SatPanel> (std::move (ctx));
        },
    };

    return def;
}

} // namespace bmo::sat

#include "Module.h"
#include "modules/opto/dsp/OptoDsp.h"
#include "modules/opto/panel/OptoPanel.h"
#include "modules/opto/params.h"
#include "modules/opto/presets/FactoryPresets.h"

namespace bmo::opto
{

const ModuleDef& module()
{
    // Lavender: the suite's other accents are Sat's orange and Util's green,
    // and this needed to read as neither. Chosen over the earlier brass gold
    // per Frosty's aesthetic direction (2026-09-06).
    static const ModuleDef def {
        kModuleId, kModuleName, kSchemaVersion,
        220, juce::Colour (0xffd4a4ff),   // brass gold -> lavender in 0.2.0
        specs(), factory(),
        [] { return createDsp(); },
        [] (ui::ModuleContext ctx) -> std::unique_ptr<ui::ModulePanel>
        {
            return std::make_unique<OptoPanel> (std::move (ctx));
        },
    };

    return def;
}

} // namespace bmo::opto

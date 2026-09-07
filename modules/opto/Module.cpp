#include "Module.h"
#include "modules/opto/dsp/OptoDsp.h"
#include "modules/opto/panel/OptoPanel.h"
#include "modules/opto/params.h"
#include "modules/opto/presets/FactoryPresets.h"

namespace bmo::opto
{

const ModuleDef& module()
{
    // Grey, and it is the one module in the suite with no colour of its own.
    //
    // This was brass gold, then lavender in 0.2.0. The panel went greyscale in
    // 0.2.2 so that the only colour on it could mean "engaged" -- red in Tele,
    // amber in Stressed, see OptoPanel.h -- which left the accent used by
    // nothing but the header bar and the rack slot bar above it. A lavender
    // stripe over a grey faceplate read as a leftover, because it was one.
    //
    // `accent` is only ever a UI colour; nothing in the schema or the state
    // depends on it, and no test pins it.
    static const ModuleDef def {
        kModuleId, kModuleName, kSchemaVersion,
        220, ui::tokens().neutral,   // brass gold -> lavender 0.2.0 -> grey 0.2.2
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

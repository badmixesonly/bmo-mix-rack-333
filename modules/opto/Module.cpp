#include "Module.h"
#include "modules/opto/dsp/OptoDsp.h"
#include "modules/opto/panel/OptoPanel.h"
#include "modules/opto/params.h"
#include "modules/opto/presets/FactoryPresets.h"

namespace bmo::opto
{

const ModuleDef& module()
{
    // A warm brass gold: the suite's other accents are Sat's orange and
    // Util's green, and this needed to read as neither -- something closer
    // to the vintage-hardware colour this module is modelled on.
    static const ModuleDef def {
        kModuleId, kModuleName, kSchemaVersion,
        220, juce::Colour (0xffc9a227),
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

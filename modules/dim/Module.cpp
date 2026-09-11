#include "Module.h"
#include "modules/dim/dsp/DimDsp.h"
#include "modules/dim/panel/DimPanel.h"
#include "modules/dim/params.h"
#include "modules/dim/presets/FactoryPresets.h"

namespace bmo::dim
{

const ModuleDef& module()
{
    // The lavender BMO Opto carried until 0.2.2 and gave up when its panel went
    // greyscale -- see modules/opto/Module.cpp, which explains why a lavender
    // stripe over a grey faceplate read as a leftover. Nothing has used it
    // since, and the Palette Book's accent table still listing it against Opto
    // is a stale document rather than a claim on the colour.
    //
    // It is also the best-separated hue left: 64.4 degrees from BMO EQ's pink,
    // its nearest neighbour, where the teal this module was going to take (now
    // held for BMO DEQ -- see products/AGENTS.md) sat 26.8 from the utility
    // azure that appears on every panel in the suite. On
    // the dark plate it reads 6.80:1 raw.
    //
    // 220 to match BMO Opto, whose panel this one is built on.
    static const ModuleDef def {
        kModuleId, kModuleName, kSchemaVersion,
        220, juce::Colour (0xffd4a4ff),
        specs(), factory(),
        [] { return createDsp(); },
        [] (ui::ModuleContext ctx) -> std::unique_ptr<ui::ModulePanel>
        {
            return std::make_unique<DimPanel> (std::move (ctx));
        },
    };

    return def;
}

} // namespace bmo::dim

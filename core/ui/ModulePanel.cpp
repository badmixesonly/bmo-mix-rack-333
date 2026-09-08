#include "ModulePanel.h"
#include "core/product/ModuleDef.h"

namespace bmo::ui
{

// Out of line, and this file exists only for that. ModuleDef.h includes
// ModulePanel.h, so the header can only forward-declare ModuleDef -- which is
// enough to hold a reference to one and not enough to read `def.accent` off
// it. The three panels that used to paint their own rules each included
// ModuleDef.h and so never met this.
void ModulePanel::paintRules (juce::Graphics& g) const
{
    for (const auto& r : rules)
    {
        if (r.text.isEmpty())
            drawRule (g, r.row);
        else
            drawRuleLegend (g, r.row, r.text, context.def.accent);
    }
}

} // namespace bmo::ui

#pragma once

#include "core/dsp/ModuleDsp.h"
#include "core/state/ParamSpec.h"
#include "core/ui/ModulePanel.h"
#include <functional>
#include <memory>

namespace bmo
{

/** Everything a product needs to know about a module: enough to run it as its
    own plugin, and enough for the rack to put it in a slot.

    A module adds itself to the suite by providing one of these -- see
    modules/AGENTS.md for the checklist.
*/
struct ModuleDef
{
    const char* id;             ///< "eq": in state files and rack presets, never changes
    const char* name;           ///< "BMO EQ"
    int schemaVersion;          ///< bumped when the spec list changes shape
    int designWidth;            ///< panel width in the rack and standalone, px
    juce::Colour accent;        ///< the module's own colour

    const ParamSpecs& specs;
    const std::vector<FactoryPreset>& factoryPresets;

    std::function<std::unique_ptr<ModuleDsp>()> createDsp;
    std::function<std::unique_ptr<ui::ModulePanel> (ui::ModuleContext)> createPanel;

    int numParams() const noexcept { return (int) specs.size(); }
};

} // namespace bmo

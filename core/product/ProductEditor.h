#pragma once

#include "SingleModuleProcessor.h"
#include "core/ui/PresetBar.h"
#include "core/ui/ProductHeader.h"

namespace bmo
{

/** The window of a standalone product: header, preset strip, the module's
    panel. Laid out once at the module's design size and scaled as a whole,
    so knobs, legends, fonts and spacing keep their proportions at any size.
*/
class ProductEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit ProductEditor (SingleModuleProcessor&);
    ~ProductEditor() override;

    void resized() override;

    static constexpr int kDesignHeight = ui::ProductHeader::kHeight + 24 + ui::ModulePanel::kContentHeight;

private:
    void timerCallback() override;

    SingleModuleProcessor& proc;
    ui::BmoLookAndFeel lookAndFeel;

    /** Everything at design size; the editor scales this. */
    juce::Component plate;
    ui::ProductHeader header;
    ui::PresetBar presetBar;
    std::unique_ptr<ui::ModulePanel> panel;

    int designWidth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProductEditor)
};

} // namespace bmo

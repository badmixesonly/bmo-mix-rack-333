#pragma once

#include "RackProcessor.h"
#include "core/ui/PresetBar.h"
#include "core/ui/ProductHeader.h"

namespace bmo
{

/** The rack's window: a header with the rack's own preset strip, then the
    modules side by side, each under a slot header that names it and moves
    it, and a strip on the right for adding another.

    The window is as wide as the modules in it, so it grows and shrinks as
    the chain changes. Everything is laid out at design size and scaled as a
    whole, like the standalone products.
*/
class RackEditor final : public juce::AudioProcessorEditor,
                         private RackProcessor::Listener,
                         private juce::Timer
{
public:
    explicit RackEditor (RackProcessor&);
    ~RackEditor() override;

    void resized() override;

    static constexpr int kHeader    = ui::ProductHeader::kHeight;
    static constexpr int kSlotBar   = 24;
    static constexpr int kAddStrip  = 40;
    static constexpr int kMinWidth  = 300;   ///< room for the header when empty
    static constexpr int kDesignHeight = kHeader + kSlotBar + ui::ModulePanel::kContentHeight;

private:
    //==========================================================================
    /** The bar over a module in a slot: its name (click for the menu),
        left, right, remove. */
    class SlotBar final : public juce::Component
    {
    public:
        SlotBar (RackEditor&, int slot);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void showMenu();

        RackEditor& owner;
        const int slot;
        juce::TextButton name, left { "<" }, right { ">" }, remove { "x" };
    };

    /** The strip on the right: a "+" that offers the registry. */
    class AddStrip final : public juce::Component
    {
    public:
        explicit AddStrip (RackEditor&);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        RackEditor& owner;
        juce::TextButton add { "+" };
    };

    struct SlotView
    {
        std::unique_ptr<SlotBar> bar;
        std::unique_ptr<ui::ModulePanel> panel;
    };

    void rackChainWillChange() override;
    void rackChainChanged() override;
    void timerCallback() override;

    void rebuildViews();
    void layoutPlate();
    int designWidth() const;

    void showModuleMenu (juce::Component& target, std::function<void (const ModuleDef&)>);

    RackProcessor& proc;
    ui::BmoLookAndFeel lookAndFeel;

    juce::Component plate;
    ui::ProductHeader header;
    ui::PresetBar presetBar;
    std::vector<SlotView> views;
    AddStrip addStrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackEditor)
};

} // namespace bmo
